#include "WorkspacePlacementController.hpp"

#include "MonitorState.hpp"
#include "WorkspaceState.hpp"

#include "../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../debug/log/Logger.hpp"
#include "../desktop/Workspace.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/state/ViewState.hpp"
#include "../desktop/state/GlobalWindowController.hpp"
#include "../desktop/view/Window.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../output/Monitor.hpp"
#include "../layout/target/Target.hpp"
#include "../layout/LayoutManager.hpp"
#include "../layout/space/Space.hpp"
#include "../managers/EventManager.hpp"
#include "../pointer/PointerManager.hpp"
#include "../event/EventBus.hpp"
#include "../animation/WorkspaceAnimationController.hpp"
#include "../render/Renderer.hpp"

#include <ranges>

using namespace State;

UP<CWorkspacePlacementController>& State::workspacePlacementController() {
    static UP<CWorkspacePlacementController> p = makeUnique<CWorkspacePlacementController>();
    return p;
}

void CWorkspacePlacementController::ensurePersistentWorkspacesPresent(PHLWORKSPACE pWorkspace, const FMoveWorkspace& moveWorkspace) const {
    ensurePersistentWorkspacesPresent(Config::workspaceRuleMgr()->getAllWorkspaceRules(), pWorkspace, moveWorkspace);
}

void CWorkspacePlacementController::ensurePersistentWorkspacesPresent(const std::vector<SP<Config::CWorkspaceRule>>& rules, PHLWORKSPACE pWorkspace,
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
            WORKSPACEID id     = rule.m_workspaceId;
            std::string wsname = rule.m_workspaceName;

            if (id == WORKSPACE_INVALID) {
                const auto R = getWorkspaceIDNameFromString(rule.m_workspaceString);
                id           = R.id;
                wsname       = R.name;
            }

            if (id == WORKSPACE_INVALID) {
                Log::logger->log(Log::ERR, "ensurePersistentWorkspacesPresent: couldn't resolve id for workspace {}", rule.m_workspaceString);
                continue;
            }
            PWORKSPACE = State::workspaceState()->query().id(id).run();
            if (!PMONITOR)
                PMONITOR = Desktop::focusState()->monitor();

            if (!PWORKSPACE)
                PWORKSPACE = State::workspaceState()->create(id, PMONITOR->m_id, wsname, false);
        }

        if (!PMONITOR) {
            Log::logger->log(Log::ERR, "ensurePersistentWorkspacesPresent: couldn't resolve monitor for {}, skipping", rule.m_monitor);
            continue;
        }

        if (PWORKSPACE)
            PWORKSPACE->setPersistent(true);

        if (!pWorkspace)
            persistentFound.emplace_back(PWORKSPACE);

        if (PWORKSPACE) {
            if (PWORKSPACE->m_monitor == PMONITOR) {
                Log::logger->log(Log::DEBUG, "ensurePersistentWorkspacesPresent: workspace persistent {} already on {}", rule.m_workspaceString, PMONITOR->m_name);

                continue;
            }

            Log::logger->log(Log::DEBUG, "ensurePersistentWorkspacesPresent: workspace persistent {} not on {}, moving", rule.m_workspaceString, PMONITOR->m_name);
            moveWorkspace(PWORKSPACE, PMONITOR, false);
            continue;
        }
    }

    if (!pWorkspace) {
        // check non-persistent and downgrade if workspace is no longer persistent
        std::vector<PHLWORKSPACEREF> toDowngrade;
        for (auto& w : State::workspaceState()->workspaces()) {
            if (!w->isPersistent())
                continue;

            if (std::ranges::contains(persistentFound, w.lock()))
                continue;

            toDowngrade.emplace_back(w);
        }

        for (auto& ws : toDowngrade) {
            ws->setPersistent(false);
        }
    }
}

void CWorkspacePlacementController::ensureWorkspacesOnAssignedMonitors(const FMoveWorkspace& moveWorkspace) const {
    for (auto const& ws : State::workspaceState()->workspacesCopy()) {
        if (!valid(ws) || ws->m_isSpecialWorkspace)
            continue;

        const auto RULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(ws);
        if (!RULE || RULE->m_monitor.empty())
            continue;

        const auto PMONITOR = State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).configString(RULE->m_monitor).run();
        if (!PMONITOR)
            continue;

        if (ws->m_monitor == PMONITOR)
            continue;

        Log::logger->log(Log::DEBUG, "ensureWorkspacesOnAssignedMonitors: moving workspace {} to {}", ws->m_name, PMONITOR->m_name);
        moveWorkspace(ws, PMONITOR, true);
    }
}

void CWorkspacePlacementController::swapActiveWorkspaces(PHLMONITOR pMonitorA, PHLMONITOR pMonitorB) const {
    const auto PWORKSPACEA = pMonitorA->m_activeWorkspace;
    const auto PWORKSPACEB = pMonitorB->m_activeWorkspace;

    PWORKSPACEA->m_monitor = pMonitorB;
    PWORKSPACEA->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == PWORKSPACEA) {
            if (w->m_pinned) {
                w->m_workspace = PWORKSPACEB;
                continue;
            }

            w->m_monitor = pMonitorB;

            // additionally, move floating and fs windows manually
            if (w->m_isFloating)
                w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-pMonitorA->m_position + pMonitorB->m_position));

            if (w->isFullscreen()) {
                *w->m_realPosition = pMonitorB->m_position;
                *w->m_realSize     = pMonitorB->m_size;
            }

            w->updateToplevel();
        }
    }

    PWORKSPACEB->m_monitor = pMonitorA;
    PWORKSPACEB->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == PWORKSPACEB) {
            if (w->m_pinned) {
                w->m_workspace = PWORKSPACEA;
                continue;
            }

            w->m_monitor = pMonitorA;

            // additionally, move floating and fs windows manually
            if (w->m_isFloating)
                w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-pMonitorB->m_position + pMonitorA->m_position));

            if (w->isFullscreen()) {
                *w->m_realPosition = pMonitorA->m_position;
                *w->m_realSize     = pMonitorA->m_size;
            }

            w->updateToplevel();
        }
    }

    pMonitorA->m_activeWorkspace = PWORKSPACEB;
    pMonitorB->m_activeWorkspace = PWORKSPACEA;

    g_layoutManager->recalculateMonitor(pMonitorA);
    g_layoutManager->recalculateMonitor(pMonitorB);

    g_pHyprRenderer->damageMonitor(pMonitorB);
    g_pHyprRenderer->damageMonitor(pMonitorA);

    Animation::Workspace::setFullscreenFadeAnimation(PWORKSPACEB,
                                                     PWORKSPACEB->m_hasFullscreenWindow ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);
    Animation::Workspace::setFullscreenFadeAnimation(PWORKSPACEA,
                                                     PWORKSPACEA->m_hasFullscreenWindow ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);

    if (pMonitorA->m_id == Desktop::focusState()->monitor()->m_id || pMonitorB->m_id == Desktop::focusState()->monitor()->m_id) {
        const auto LASTWIN = pMonitorA->m_id == Desktop::focusState()->monitor()->m_id ? PWORKSPACEB->getLastFocusedWindow() : PWORKSPACEA->getLastFocusedWindow();
        Desktop::focusState()->fullWindowFocus(
            LASTWIN ? LASTWIN :
                      (Desktop::viewState()->hitTest().windowAt(g_pInputManager->getMouseCoordsInternal(),
                                                                Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING)),
            Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);

        const auto PNEWWORKSPACE = pMonitorA->m_id == Desktop::focusState()->monitor()->m_id ? PWORKSPACEB : PWORKSPACEA;
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "workspace", .data = PNEWWORKSPACE->m_name});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "workspacev2", .data = std::format("{},{}", PNEWWORKSPACE->m_id, PNEWWORKSPACE->m_name)});
        Event::bus()->m_events.workspace.active.emit(PNEWWORKSPACE);
    }

    // events
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = PWORKSPACEA->m_name + "," + pMonitorB->m_name});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", PWORKSPACEA->m_id, PWORKSPACEA->m_name, pMonitorB->m_name)});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = PWORKSPACEB->m_name + "," + pMonitorA->m_name});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", PWORKSPACEB->m_id, PWORKSPACEB->m_name, pMonitorA->m_name)});

    Event::bus()->m_events.workspace.moveToMonitor.emit(PWORKSPACEA, pMonitorB);
    Event::bus()->m_events.workspace.moveToMonitor.emit(PWORKSPACEB, pMonitorA);
}

void CWorkspacePlacementController::moveWorkspaceToMonitor(PHLWORKSPACE pWorkspace, PHLMONITOR pMonitor, bool noWarpCursor) const {
    static auto PHIDESPECIALONWORKSPACECHANGE = CConfigValue<Config::INTEGER>("binds:hide_special_on_workspace_change");

    if (!pWorkspace || !pMonitor)
        return;

    if (pWorkspace->m_monitor == pMonitor)
        return;

    Log::logger->log(Log::DEBUG, "moveWorkspaceToMonitor: Moving {} to monitor {}", pWorkspace->m_id, pMonitor->m_id);

    const auto POLDMON = pWorkspace->m_monitor.lock();

    if (pWorkspace->m_isSpecialWorkspace && POLDMON && POLDMON->m_activeSpecialWorkspace == pWorkspace) {
        pMonitor->setSpecialWorkspace(pWorkspace);
        return;
    }

    const bool SWITCHINGISACTIVE = POLDMON ? POLDMON->m_activeWorkspace == pWorkspace : false;

    // fix old mon
    WORKSPACEID nextWorkspaceOnMonitorID = WORKSPACE_INVALID;
    if (!SWITCHINGISACTIVE)
        nextWorkspaceOnMonitorID = pWorkspace->m_id;
    else {
        PHLWORKSPACE newWorkspace; // for holding a ref to the new workspace that might be created

        for (auto const& w : State::workspaceState()->workspaces()) {
            if (w->m_monitor == POLDMON && w->m_id != pWorkspace->m_id && !w->m_isSpecialWorkspace) {
                nextWorkspaceOnMonitorID = w->m_id;
                break;
            }
        }

        if (nextWorkspaceOnMonitorID == WORKSPACE_INVALID) {
            nextWorkspaceOnMonitorID = 1;

            while (State::workspaceState()->query().id(nextWorkspaceOnMonitorID).run() || [&]() -> bool {
                const auto B = Config::workspaceRuleMgr()->getBoundMonitorForWS(std::to_string(nextWorkspaceOnMonitorID));
                return B && B != POLDMON;
            }())
                nextWorkspaceOnMonitorID++;

            Log::logger->log(Log::DEBUG, "moveWorkspaceToMonitor: Plugging gap with new {}", nextWorkspaceOnMonitorID);

            if (POLDMON)
                newWorkspace = State::workspaceState()->create(nextWorkspaceOnMonitorID, POLDMON->m_id);
        }

        Log::logger->log(Log::DEBUG, "moveWorkspaceToMonitor: Plugging gap with existing {}", nextWorkspaceOnMonitorID);
        if (POLDMON)
            POLDMON->changeWorkspace(nextWorkspaceOnMonitorID, false, true, true);
    }

    // move the workspace
    pWorkspace->m_monitor = pMonitor;
    pWorkspace->m_space->recheckWorkArea();
    pWorkspace->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == pWorkspace) {
            if (w->m_pinned) {
                w->m_workspace = State::workspaceState()->query().id(nextWorkspaceOnMonitorID).run();
                continue;
            }

            w->m_monitor = pMonitor;

            // additionally, move floating and fs windows manually
            if (w->m_isMapped && !w->isHidden()) {
                if (POLDMON) {
                    if (w->m_isFloating)
                        w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-POLDMON->m_position + pMonitor->m_position));

                    if (w->isFullscreen()) {
                        *w->m_realPosition = pMonitor->m_position;
                        *w->m_realSize     = pMonitor->m_size;
                    }
                } else
                    w->layoutTarget()->setPositionGlobal(CBox{Vector2D{
                                                                  (pMonitor->m_size.x != 0) ? sc<int>(w->m_realPosition->goal().x) % sc<int>(pMonitor->m_size.x) : 0,
                                                                  (pMonitor->m_size.y != 0) ? sc<int>(w->m_realPosition->goal().y) % sc<int>(pMonitor->m_size.y) : 0,
                                                              },
                                                              w->layoutTarget()->position().size()});
            }

            w->updateToplevel();
        }
    }

    if (SWITCHINGISACTIVE && POLDMON == Desktop::focusState()->monitor()) { // if it was active, preserve its' status. If it wasn't, don't.
        Log::logger->log(Log::DEBUG, "moveWorkspaceToMonitor: SWITCHINGISACTIVE, active {} -> {}", pMonitor->activeWorkspaceID(), pWorkspace->m_id);

        if (valid(pMonitor->m_activeWorkspace)) {
            pMonitor->m_activeWorkspace->m_visible = false;
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
        pWorkspace->m_visible = true;

        if (!noWarpCursor)
            Pointer::mgr()->warpTo(pMonitor->m_position + pMonitor->m_transformedSize / 2.F);

        g_pInputManager->sendMotionEventsToFocused();
    }

    // finalize
    if (POLDMON) {
        g_layoutManager->recalculateMonitor(POLDMON);
        if (valid(POLDMON->m_activeWorkspace))
            Animation::Workspace::setFullscreenFadeAnimation(
                POLDMON->m_activeWorkspace, POLDMON->m_activeWorkspace->m_hasFullscreenWindow ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);
        Desktop::globalWindowController()->updateSuspendedStates();
    }

    Animation::Workspace::setFullscreenFadeAnimation(pWorkspace,
                                                     pWorkspace->m_hasFullscreenWindow ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);
    Desktop::globalWindowController()->updateSuspendedStates();

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = pWorkspace->m_name + "," + pMonitor->m_name});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", pWorkspace->m_id, pWorkspace->m_name, pMonitor->m_name)});

    Event::bus()->m_events.workspace.moveToMonitor.emit(pWorkspace, pMonitor);
}
