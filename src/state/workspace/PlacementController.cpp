#include "PlacementController.hpp"

#include "../MonitorState.hpp"
#include "Resolver.hpp"
#include "State.hpp"

#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../workspace/HLWorkspace.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/state/WindowState.hpp"
#include "../../desktop/state/ViewState.hpp"
#include "../../desktop/state/GlobalWindowController.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../output/Monitor.hpp"
#include "../../layout/target/Target.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../layout/space/Space.hpp"
#include "../../ipc/s2/S2.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../pointer/PointerManager.hpp"
#include "../../event/EventBus.hpp"
#include "../../animation/WorkspaceAnimationController.hpp"
#include "../../render/Renderer.hpp"
#include "../../workspace/RegularWorkspace.hpp"
#include "../../workspace/query/Query.hpp"

#include <ranges>

using namespace State::Workspace;

UP<CPlacementController>& State::Workspace::placementController() {
    static UP<CPlacementController> p = makeUnique<CPlacementController>();
    return p;
}

void CPlacementController::ensurePersistentWorkspacesPresent(PHLWORKSPACE pWorkspace, const FMoveWorkspace& moveWorkspace) const {
    ensurePersistentWorkspacesPresent(Config::workspaceRuleMgr()->getAllWorkspaceRules(), pWorkspace, moveWorkspace);
}

void CPlacementController::ensurePersistentWorkspacesPresent(const std::vector<SP<Config::CWorkspaceRule>>& rules, PHLWORKSPACE pWorkspace,
                                                             const FMoveWorkspace& moveWorkspace) const {
    if (!Desktop::focusState()->monitor())
        return;

    std::vector<PHLWORKSPACE> persistentFound;

    for (const auto& rulePtr : rules) {
        if (!rulePtr->isEnabled() || !rulePtr->m_isPersistent.value_or(false))
            continue;

        const auto&  rule = *rulePtr;

        PHLWORKSPACE PWORKSPACE = nullptr;
        if (pWorkspace) {
            if (pWorkspace->matchesStaticSelector(rule.m_workspaceString))
                PWORKSPACE = pWorkspace;
            else
                continue;
        }

        auto PMONITOR = State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).configString(rule.m_monitor).run();

        if (!rule.m_monitor.empty() && !PMONITOR)
            continue; // don't do anything yet, as the monitor is not yet present.

        if (!PWORKSPACE) {
            const auto TARGET = resolver()->getWorkspaceTargetFromString(rule.m_workspaceString);
            if (!TARGET.valid()) {
                LOG(Log::ERR, "ensurePersistentWorkspacesPresent: couldn't resolve workspace {}", rule.m_workspaceString);
                continue;
            }
            if (TARGET.type == ::Workspace::eWorkspaceType::SPECIAL)
                continue;
            PWORKSPACE = State::Workspace::state()->find(TARGET);
            if (!PMONITOR)
                PMONITOR = Desktop::focusState()->monitor();

            if (!PWORKSPACE)
                PWORKSPACE = State::Workspace::state()->create(TARGET, PMONITOR, false);
        }

        const auto REGULAR = dynamicPointerCast<::Workspace::CRegularWorkspace>(PWORKSPACE);
        if (!REGULAR)
            continue;

        if (!PMONITOR) {
            LOG(Log::ERR, "ensurePersistentWorkspacesPresent: couldn't resolve monitor for {}, skipping", rule.m_monitor);
            continue;
        }

        REGULAR->setPersistent(true);

        if (!pWorkspace)
            persistentFound.emplace_back(PWORKSPACE);

        if (PWORKSPACE) {
            if (PWORKSPACE->m_monitor == PMONITOR) {
                LOG(Log::DEBUG, "ensurePersistentWorkspacesPresent: workspace persistent {} already on {}", rule.m_workspaceString, PMONITOR->m_name);

                continue;
            }

            LOG(Log::DEBUG, "ensurePersistentWorkspacesPresent: workspace persistent {} not on {}, moving", rule.m_workspaceString, PMONITOR->m_name);
            moveWorkspace(PWORKSPACE, PMONITOR, false);
            continue;
        }
    }

    if (!pWorkspace) {
        // check non-persistent and downgrade if workspace is no longer persistent
        std::vector<PHLWORKSPACEREF> toDowngrade;
        for (auto& w : state()->workspaces()) {
            const auto REGULAR = dynamicPointerCast<::Workspace::CRegularWorkspace>(w.lock());
            if (!REGULAR || !REGULAR->isPersistent())
                continue;

            if (std::ranges::contains(persistentFound, w.lock()))
                continue;

            toDowngrade.emplace_back(w);
        }

        for (auto& ws : toDowngrade) {
            const auto REGULAR = dynamicPointerCast<::Workspace::CRegularWorkspace>(ws.lock());
            if (REGULAR)
                REGULAR->setPersistent(false);
        }
    }
}

void CPlacementController::ensureWorkspacesOnAssignedMonitors(const FMoveWorkspace& moveWorkspace) const {
    for (auto const& ws : state()->workspacesCopy()) {
        if (!ws || ws->type() == ::Workspace::eWorkspaceType::SPECIAL)
            continue;

        const auto RULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(ws);
        if (!RULE || RULE->m_monitor.empty())
            continue;

        const auto PMONITOR = State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).configString(RULE->m_monitor).run();
        if (!PMONITOR)
            continue;

        if (ws->m_monitor == PMONITOR)
            continue;

        LOG(Log::DEBUG, "ensureWorkspacesOnAssignedMonitors: moving workspace {} to {}", ws->displayName(), PMONITOR->m_name);
        moveWorkspace(ws, PMONITOR, true);
    }
}

void CPlacementController::swapActiveWorkspaces(PHLMONITOR pMonitorA, PHLMONITOR pMonitorB) const {
    const auto PWORKSPACEA = pMonitorA->m_activeWorkspace;
    const auto PWORKSPACEB = pMonitorB->m_activeWorkspace;

    PWORKSPACEA->m_monitor = pMonitorB;
    PWORKSPACEA->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == PWORKSPACEA) {
            if (w->m_state & Desktop::View::WINDOW_STATE_PINNED) {
                w->m_workspace = PWORKSPACEB;
                continue;
            }

            w->m_monitor = pMonitorB;

            // additionally, move floating and fs windows manually
            if (w->isFloating())
                w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-pMonitorA->m_position + pMonitorB->m_position));

            if (Fullscreen::controller()->isFullscreen(w))
                w->setBox({pMonitorB->m_position, pMonitorB->m_size});

            w->updateToplevel();
        }
    }

    PWORKSPACEB->m_monitor = pMonitorA;
    PWORKSPACEB->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == PWORKSPACEB) {
            if (w->m_state & Desktop::View::WINDOW_STATE_PINNED) {
                w->m_workspace = PWORKSPACEA;
                continue;
            }

            w->m_monitor = pMonitorA;

            // additionally, move floating and fs windows manually
            if (w->isFloating())
                w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-pMonitorB->m_position + pMonitorA->m_position));

            if (Fullscreen::controller()->isFullscreen(w))
                w->setBox({pMonitorA->m_position, pMonitorA->m_size});

            w->updateToplevel();
        }
    }

    pMonitorA->m_activeWorkspace = PWORKSPACEB;
    pMonitorB->m_activeWorkspace = PWORKSPACEA;

    g_layoutManager->recalculateMonitor(pMonitorA);
    g_layoutManager->recalculateMonitor(pMonitorB);

    g_pHyprRenderer->damageMonitor(pMonitorB);
    g_pHyprRenderer->damageMonitor(pMonitorA);

    Animation::Workspace::setFullscreenFadeAnimation(
        PWORKSPACEB, Fullscreen::controller()->hasFullscreen(PWORKSPACEB) ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);
    Animation::Workspace::setFullscreenFadeAnimation(
        PWORKSPACEA, Fullscreen::controller()->hasFullscreen(PWORKSPACEA) ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);

    if (pMonitorA->m_id == Desktop::focusState()->monitor()->m_id || pMonitorB->m_id == Desktop::focusState()->monitor()->m_id) {
        const auto LASTWIN = pMonitorA->m_id == Desktop::focusState()->monitor()->m_id ? PWORKSPACEB->getLastFocusedWindow() : PWORKSPACEA->getLastFocusedWindow();
        Desktop::focusState()->fullWindowFocus(
            LASTWIN ? LASTWIN :
                      (Desktop::viewState()->hitTest().windowAt(g_pInputManager->getMouseCoordsInternal(),
                                                                Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING)),
            Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);

        const auto PNEWWORKSPACE = pMonitorA->m_id == Desktop::focusState()->monitor()->m_id ? PWORKSPACEB : PWORKSPACEA;
        IPC::Socket2::sock()->postEvent({.event = "workspace", .data = PNEWWORKSPACE->displayName()});
        IPC::Socket2::sock()->postEvent({.event = "workspacev2", .data = std::format("{},{}", ::Workspace::selector(*PNEWWORKSPACE), PNEWWORKSPACE->displayName())});
        Event::bus()->m_events.workspace.active.emit(PNEWWORKSPACE);
    }

    // events
    IPC::Socket2::sock()->postEvent({.event = "moveworkspace", .data = std::format("{},{}", PWORKSPACEA->displayName(), pMonitorB->m_name)});
    IPC::Socket2::sock()->postEvent(
        {.event = "moveworkspacev2", .data = std::format("{},{},{}", ::Workspace::selector(*PWORKSPACEA), PWORKSPACEA->displayName(), pMonitorB->m_name)});
    IPC::Socket2::sock()->postEvent({.event = "moveworkspace", .data = std::format("{},{}", PWORKSPACEB->displayName(), pMonitorA->m_name)});
    IPC::Socket2::sock()->postEvent(
        {.event = "moveworkspacev2", .data = std::format("{},{},{}", ::Workspace::selector(*PWORKSPACEB), PWORKSPACEB->displayName(), pMonitorA->m_name)});

    Event::bus()->m_events.workspace.moveToMonitor.emit(PWORKSPACEA, pMonitorB);
    Event::bus()->m_events.workspace.moveToMonitor.emit(PWORKSPACEB, pMonitorA);
}

void CPlacementController::moveWorkspaceToMonitor(PHLWORKSPACE pWorkspace, PHLMONITOR pMonitor, bool noWarpCursor, bool carryFocus) const {
    static auto PHIDESPECIALONWORKSPACECHANGE = CConfigValue<Config::INTEGER>("binds:hide_special_on_workspace_change");

    if (!pWorkspace || !pMonitor)
        return;

    if (pWorkspace->m_monitor == pMonitor)
        return;

    LOG(Log::DEBUG, "moveWorkspaceToMonitor: Moving {} to monitor {}", pWorkspace->addressableName(), pMonitor->m_id);

    const auto POLDMON = pWorkspace->m_monitor.lock();

    if (pWorkspace->type() == ::Workspace::eWorkspaceType::SPECIAL && POLDMON && POLDMON->m_activeSpecialWorkspace == pWorkspace) {
        pMonitor->setSpecialWorkspace(pWorkspace);
        return;
    }

    const bool SWITCHINGISACTIVE = POLDMON ? POLDMON->m_activeWorkspace == pWorkspace : false;

    // fix old mon
    PHLWORKSPACE nextWorkspaceOnMonitor;
    if (!SWITCHINGISACTIVE)
        nextWorkspaceOnMonitor = pWorkspace;
    else {
        PHLWORKSPACE newWorkspace; // for holding a ref to the new workspace that might be created

        for (auto const& w : state()->workspaces()) {
            if (w->m_monitor == POLDMON && w.lock() != pWorkspace && w->type() != ::Workspace::eWorkspaceType::SPECIAL) {
                nextWorkspaceOnMonitor = w.lock();
                break;
            }
        }

        if (!nextWorkspaceOnMonitor) {
            ::Workspace::WorkspaceIDContainer nextWorkspaceOnMonitorID = 1;

            while (state()->query().numbered(::Workspace::SWorkspaceNumberedID{nextWorkspaceOnMonitorID}).run() || [&]() -> bool {
                const auto B = Config::workspaceRuleMgr()->getBoundMonitorForWS(std::to_string(nextWorkspaceOnMonitorID));
                return B && B != POLDMON;
            }())
                nextWorkspaceOnMonitorID++;

            LOG(Log::DEBUG, "moveWorkspaceToMonitor: Plugging gap with new {}", nextWorkspaceOnMonitorID);

            if (POLDMON)
                newWorkspace = state()->createNumbered(::Workspace::SWorkspaceNumberedID{nextWorkspaceOnMonitorID}, POLDMON);
            nextWorkspaceOnMonitor = newWorkspace;
        }

        LOG(Log::DEBUG, "moveWorkspaceToMonitor: Plugging gap with existing {}", nextWorkspaceOnMonitor ? nextWorkspaceOnMonitor->addressableName() : "none");
        if (POLDMON)
            POLDMON->changeWorkspace(nextWorkspaceOnMonitor, false, true, carryFocus || POLDMON != Desktop::focusState()->monitor());
    }

    // move the workspace
    pWorkspace->m_monitor = pMonitor;
    pWorkspace->space()->recheckWorkArea();
    pWorkspace->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == pWorkspace) {
            if (w->m_state & Desktop::View::WINDOW_STATE_PINNED) {
                w->m_workspace = nextWorkspaceOnMonitor;
                continue;
            }

            w->m_monitor = pMonitor;

            // additionally, move floating and fs windows manually
            if (w->mapped() && !w->isHidden()) {
                if (POLDMON) {
                    if (w->isFloating())
                        w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-POLDMON->m_position + pMonitor->m_position));

                    if (Fullscreen::controller()->isFullscreen(w))
                        w->setBox({pMonitor->m_position, pMonitor->m_size});
                } else
                    w->layoutTarget()->setPositionGlobal(
                        CBox{Vector2D{
                                 (pMonitor->m_size.x != 0) ? sc<int>(w->position(Desktop::View::IGeometric::GEOMETRIC_GOAL).x) % sc<int>(pMonitor->m_size.x) : 0,
                                 (pMonitor->m_size.y != 0) ? sc<int>(w->position(Desktop::View::IGeometric::GEOMETRIC_GOAL).y) % sc<int>(pMonitor->m_size.y) : 0,
                             },
                             w->layoutTarget()->position().size()});
            }

            w->updateToplevel();
        }
    }

    if (carryFocus && SWITCHINGISACTIVE && POLDMON == Desktop::focusState()->monitor()) { // if it was active, preserve its' status. If it wasn't, don't.
        LOG(Log::DEBUG, "moveWorkspaceToMonitor: SWITCHINGISACTIVE, active {} -> {}", pMonitor->m_activeWorkspace ? pMonitor->m_activeWorkspace->addressableName() : "none",
            pWorkspace->addressableName());

        if (pMonitor->m_activeWorkspace) {
            pMonitor->m_activeWorkspace->setVisible(false);
            Animation::Workspace::startAnimation(pWorkspace, Animation::Workspace::ANIMATION_TYPE_OUT, false);
        }

        if (*PHIDESPECIALONWORKSPACECHANGE)
            pMonitor->setSpecialWorkspace(nullptr);

        Desktop::focusState()->rawMonitorFocus(pMonitor);

        auto oldWorkspace           = pMonitor->m_activeWorkspace;
        pMonitor->m_activeWorkspace = pWorkspace;

        if (oldWorkspace)
            oldWorkspace->m_events.activeChanged.emit();

        pWorkspace->m_events.activeChanged.emit();

        g_layoutManager->recalculateMonitor(pMonitor);
        g_pHyprRenderer->damageMonitor(pMonitor);

        Animation::Workspace::startAnimation(pWorkspace, Animation::Workspace::ANIMATION_TYPE_IN, true, true);
        pWorkspace->setVisible(true);

        if (!noWarpCursor)
            Pointer::mgr()->warpTo(pMonitor->m_position + pMonitor->m_transformedSize / 2.F);

        g_pInputManager->sendMotionEventsToFocused();
    }

    // finalize
    if (POLDMON) {
        g_layoutManager->recalculateMonitor(POLDMON);
        if (POLDMON->m_activeWorkspace)
            Animation::Workspace::setFullscreenFadeAnimation(POLDMON->m_activeWorkspace,
                                                             Fullscreen::controller()->hasFullscreen(POLDMON->m_activeWorkspace) ? Animation::Workspace::ANIMATION_TYPE_IN :
                                                                                                                                   Animation::Workspace::ANIMATION_TYPE_OUT);
        Desktop::globalWindowController()->updateSuspendedStates();
    }

    Animation::Workspace::setFullscreenFadeAnimation(
        pWorkspace, Fullscreen::controller()->hasFullscreen(pWorkspace) ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);
    Desktop::globalWindowController()->updateSuspendedStates();

    // event
    IPC::Socket2::sock()->postEvent({.event = "moveworkspace", .data = std::format("{},{}", pWorkspace->displayName(), pMonitor->m_name)});
    IPC::Socket2::sock()->postEvent({.event = "moveworkspacev2", .data = std::format("{},{},{}", ::Workspace::selector(*pWorkspace), pWorkspace->displayName(), pMonitor->m_name)});

    Event::bus()->m_events.workspace.moveToMonitor.emit(pWorkspace, pMonitor);
}
