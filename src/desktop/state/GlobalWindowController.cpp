#include "GlobalWindowController.hpp"

#include "WindowState.hpp"
#include "../view/window/Window.hpp"
#include "../view/window/WindowGroupMembership.hpp"
#include "../view/window/WindowPresentation.hpp"
#include "../view/Group.hpp"
#include "../../output/Monitor.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../layout/target/Target.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"

using namespace Desktop;

UP<CGlobalWindowController>& Desktop::globalWindowController() {
    static UP<CGlobalWindowController> p = makeUnique<CGlobalWindowController>();
    return p;
}

void CGlobalWindowController::updateAllWindowsDecorations() const {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->mapped())
            continue;

        w->presentation().refreshValues();
    }
}

void CGlobalWindowController::updateSuspendedStates() const {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->mapped())
            continue;

        w->setSuspended(w->isHidden() || !w->m_workspace || !w->m_workspace->isVisible());
    }
}

void CGlobalWindowController::moveWindowToWorkspace(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) const {
    if (!pWindow || !pWorkspace)
        return;

    if ((pWindow->m_state & Desktop::View::WINDOW_STATE_PINNED) != Desktop::View::WINDOW_STATE_NONE && pWorkspace->m_isSpecialWorkspace)
        return;

    if (pWindow->m_workspace == pWorkspace)
        return;

    const bool FULLSCREEN     = Fullscreen::controller()->isFullscreen(pWindow);
    const auto FULLSCREENMODE = Fullscreen::controller()->getFullscreenModes(pWindow).internal;
    const auto LAYOUT_AWARE   = FULLSCREEN ? Fullscreen::controller()->layoutManagedFS(pWindow) : false;
    const bool WASVISIBLE     = pWindow->m_workspace && pWindow->m_workspace->isVisible();

    if (FULLSCREEN)
        Fullscreen::controller()->setFullscreenMode(pWindow, Fullscreen::FSMODE_NONE);

    const PHLWINDOW pFirstWindowOnWorkspace   = pWorkspace->getFirstWindow();
    const int       visibleWindowsOnWorkspace = pWorkspace->getWindowCount(true, std::nullopt, true);
    const auto      POSTOMON                  = pWindow->position(Desktop::View::IGeometric::GEOMETRIC_GOAL) - (pWindow->m_monitor ? pWindow->m_monitor->m_position : Vector2D{});
    const auto      PWORKSPACEMONITOR         = pWorkspace->m_monitor.lock();

    pWindow->moveToWorkspace(pWorkspace);
    pWindow->m_monitor = pWorkspace->m_monitor;

    static auto PGROUPONMOVETOWORKSPACE = CConfigValue<Config::INTEGER>("group:group_on_movetoworkspace");
    if (*PGROUPONMOVETOWORKSPACE && visibleWindowsOnWorkspace == 1 && pFirstWindowOnWorkspace && pFirstWindowOnWorkspace != pWindow &&
        pFirstWindowOnWorkspace->grouping().group() && pWindow->grouping().canBeGroupedInto(pFirstWindowOnWorkspace->grouping().group())) {
        pFirstWindowOnWorkspace->grouping().group()->add(pWindow);
    } else {
        if (pWindow->isFloating())
            pWindow->layoutTarget()->setPositionGlobal(CBox{POSTOMON + PWORKSPACEMONITOR->m_position, pWindow->layoutTarget()->position().size()});
    }

    pWindow->updateToplevel();
    pWindow->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    pWindow->presentation().uncacheDecorations();

    if (pWindow->grouping().group())
        pWindow->grouping().group()->updateWorkspace(pWorkspace);

    g_layoutManager->newTarget(pWindow->layoutTarget(), pWorkspace->m_space);

    if (FULLSCREEN)
        Fullscreen::controller()->setFullscreenMode(pWindow, FULLSCREENMODE, std::nullopt, LAYOUT_AWARE);

    pWorkspace->updateWindows();
    if (pWindow->m_workspace)
        pWindow->m_workspace->updateWindows();
    updateSuspendedStates();

    if (!WASVISIBLE && pWindow->m_workspace && pWindow->m_workspace->isVisible()) {
        pWindow->presentation().alpha(View::WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(0.F);
        *pWindow->presentation().alpha(View::WINDOW_ALPHA_MOVE_FROM_WORKSPACE) = 1.F;
    }
}
