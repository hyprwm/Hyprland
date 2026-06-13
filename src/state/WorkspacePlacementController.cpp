#include "WorkspacePlacementController.hpp"

#include "MonitorState.hpp"
#include "WorkspaceState.hpp"

#include "../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../debug/log/Logger.hpp"
#include "../desktop/Workspace.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../output/Monitor.hpp"

#include <ranges>

using namespace State;

UP<CWorkspacePlacementController>& State::workspacePlacementController() {
    static UP<CWorkspacePlacementController> p = makeUnique<CWorkspacePlacementController>();
    return p;
}

void CWorkspacePlacementController::ensurePersistentWorkspacesPresent(PHLWORKSPACE pWorkspace, const FMoveWorkspace& moveWorkspace) const {
    ensurePersistentWorkspacesPresent(Config::workspaceRuleMgr()->getAllWorkspaceRules(), pWorkspace, moveWorkspace);
}

void CWorkspacePlacementController::ensurePersistentWorkspacesPresent(const std::vector<Config::CWorkspaceRule>& rules, PHLWORKSPACE pWorkspace,
                                                                      const FMoveWorkspace& moveWorkspace) const {
    if (!Desktop::focusState()->monitor())
        return;

    std::vector<PHLWORKSPACE> persistentFound;

    for (const auto& rule : rules) {
        if (!rule.m_isPersistent.value_or(false))
            continue;

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
