#include "WorkspaceState.hpp"

#include "MonitorState.hpp"
#include "WorkspaceQueryCore.hpp"

#include "../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../desktop/Workspace.hpp"
#include "../debug/log/Logger.hpp"

using namespace State;

UP<CWorkspaceStateTracker>& State::workspaceState() {
    static UP<CWorkspaceStateTracker> p = makeUnique<CWorkspaceStateTracker>();
    return p;
}

const std::vector<PHLWORKSPACEREF>& CWorkspaceStateTracker::workspaceRefs() const {
    return m_workspaces;
}

std::vector<SWorkspaceQueryable> CWorkspaceStateTracker::queryableWorkspaces() const {
    std::vector<SWorkspaceQueryable> queryable;
    queryable.reserve(m_workspaces.size());

    for (const auto& w : m_workspaces) {
        const auto WORKSPACE = w.lock();
        queryable.push_back({
            .id      = WORKSPACE ? WORKSPACE->m_id : WORKSPACE_INVALID,
            .name    = WORKSPACE ? std::string_view{WORKSPACE->m_name} : std::string_view{},
            .inert   = !valid(WORKSPACE),
            .special = WORKSPACE ? WORKSPACE->m_isSpecialWorkspace : false,
        });
    }

    return queryable;
}

std::vector<PHLWORKSPACE> CWorkspaceStateTracker::workspacesCopy() const {
    std::vector<PHLWORKSPACE> wsp;
    auto                      range = workspaces();
    wsp.reserve(std::ranges::distance(range));
    for (auto& r : range) {
        wsp.emplace_back(r.lock());
    }
    return wsp;
}

void CWorkspaceStateTracker::add(PHLWORKSPACE w) {
    m_workspaces.emplace_back(w);
    w->m_events.destroy.listenStatic([this, weak = PHLWORKSPACEREF{w}] { std::erase(m_workspaces, weak); });
}

void CWorkspaceStateTracker::clear() {
    m_workspaces.clear();
    m_seenMonitorWorkspaceMap.clear();
}

PHLWORKSPACE CWorkspaceStateTracker::create(const WORKSPACEID& id, const MONITORID& monid, const std::string& name, bool isEmpty) {
    const auto NAME  = name.empty() ? std::to_string(id) : name;
    auto       monID = monid;

    // check if bound
    if (const auto PMONITOR = Config::workspaceRuleMgr()->getBoundMonitorForWS(NAME); PMONITOR)
        monID = PMONITOR->m_id;

    const bool SPECIAL = CWorkspaceQueryCore::isSpecial(id);

    const auto PMONITOR = State::monitorState()->query().id(monID).run();
    if (!PMONITOR) {
        Log::logger->log(Log::ERR, "BUG THIS: No pMonitor for new workspace in CWorkspaceStateTracker::create");
        return nullptr;
    }

    const auto PWORKSPACE = CWorkspace::create(id, PMONITOR, NAME, SPECIAL, isEmpty);

    PWORKSPACE->m_alpha->setValueAndWarp(0);

    return PWORKSPACE;
}

WORKSPACEID CWorkspaceStateTracker::nextAvailableNamedWorkspace() const {
    std::vector<WORKSPACEID> persistentWorkspaceIDs;

    // Give priority to persistent workspaces to avoid any conflicts between them.
    for (auto const& rule : Config::workspaceRuleMgr()->getAllWorkspaceRules()) {
        if (!rule->isEnabled() || !rule->m_isPersistent.value_or(false))
            continue;
        persistentWorkspaceIDs.push_back(rule->m_workspaceId);
    }

    return CWorkspaceQueryCore::nextAvailableNamedWorkspace(queryableWorkspaces(), persistentWorkspaceIDs);
}

WORKSPACEID CWorkspaceStateTracker::newSpecialID() const {
    return CWorkspaceQueryCore::newSpecialID(queryableWorkspaces());
}

bool CWorkspaceStateTracker::isSpecial(const WORKSPACEID& id) const {
    return CWorkspaceQueryCore::isSpecial(id);
}

bool CWorkspaceStateTracker::idOutOfBounds(const WORKSPACEID& id) const {
    return CWorkspaceQueryCore::idOutOfBounds(queryableWorkspaces(), id);
}

void CWorkspaceStateTracker::rememberWorkspaceForMonitor(const std::string& monitor, WORKSPACEID workspace) {
    m_seenMonitorWorkspaceMap[monitor] = workspace;
}

std::optional<WORKSPACEID> CWorkspaceStateTracker::rememberedWorkspaceForMonitor(const std::string& monitor) const {
    if (!m_seenMonitorWorkspaceMap.contains(monitor))
        return std::nullopt;

    return m_seenMonitorWorkspaceMap.at(monitor);
}
