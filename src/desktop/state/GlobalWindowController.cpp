#include "GlobalWindowController.hpp"

#include "WindowState.hpp"
#include "../view/Window.hpp"
#include "../view/Group.hpp"
#include "../../output/Monitor.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../Compositor.hpp"

using namespace Desktop;

UP<CGlobalWindowController>& Desktop::globalWindowController() {
    static UP<CGlobalWindowController> p = makeUnique<CGlobalWindowController>();
    return p;
}

void CGlobalWindowController::updateAllWindowsDecorations() const {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->m_isMapped)
            continue;

        w->updateDecorationValues();
    }
}

void CGlobalWindowController::updateSuspendedStates() const {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->m_isMapped)
            continue;

        w->setSuspended(w->isHidden() || !w->m_workspace || !w->m_workspace->isVisible());
    }
}

void CGlobalWindowController::moveWindowToWorkspace(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) const {
    if (!pWindow || !pWorkspace)
        return;

    if (pWindow->m_pinned && pWorkspace->m_isSpecialWorkspace)
        return;

    if (pWindow->m_workspace == pWorkspace)
        return;

    const bool FULLSCREEN     = pWindow->isFullscreen();
    const auto FULLSCREENMODE = pWindow->m_fullscreenState.internal;
    const bool WASVISIBLE     = pWindow->m_workspace && pWindow->m_workspace->isVisible();

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    const PHLWINDOW pFirstWindowOnWorkspace   = pWorkspace->getFirstWindow();
    const int       visibleWindowsOnWorkspace = pWorkspace->getWindowCount(true, std::nullopt, true);
    const auto      POSTOMON                  = pWindow->m_realPosition->goal() - (pWindow->m_monitor ? pWindow->m_monitor->m_position : Vector2D{});
    const auto      PWORKSPACEMONITOR         = pWorkspace->m_monitor.lock();

    pWindow->moveToWorkspace(pWorkspace);
    pWindow->m_monitor = pWorkspace->m_monitor;

    static auto PGROUPONMOVETOWORKSPACE = CConfigValue<Config::INTEGER>("group:group_on_movetoworkspace");
    if (*PGROUPONMOVETOWORKSPACE && visibleWindowsOnWorkspace == 1 && pFirstWindowOnWorkspace && pFirstWindowOnWorkspace != pWindow && pFirstWindowOnWorkspace->m_group &&
        pWindow->canBeGroupedInto(pFirstWindowOnWorkspace->m_group)) {
        pFirstWindowOnWorkspace->m_group->add(pWindow);
    } else {
        if (pWindow->m_isFloating)
            pWindow->layoutTarget()->setPositionGlobal(CBox{POSTOMON + PWORKSPACEMONITOR->m_position, pWindow->layoutTarget()->position().size()});
    }

    pWindow->updateToplevel();
    pWindow->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    pWindow->uncacheWindowDecos();

    if (pWindow->m_group)
        pWindow->m_group->updateWorkspace(pWorkspace);

    g_layoutManager->newTarget(pWindow->layoutTarget(), pWorkspace->m_space);

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreenInternal(pWindow, FULLSCREENMODE);

    pWorkspace->updateWindows();
    if (pWindow->m_workspace)
        pWindow->m_workspace->updateWindows();
    updateSuspendedStates();

    if (!WASVISIBLE && pWindow->m_workspace && pWindow->m_workspace->isVisible()) {
        pWindow->alpha(View::WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(0.F);
        *pWindow->alpha(View::WINDOW_ALPHA_MOVE_FROM_WORKSPACE) = 1.F;
    }
}
