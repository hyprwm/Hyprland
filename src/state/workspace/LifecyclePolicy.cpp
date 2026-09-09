#include "LifecyclePolicy.hpp"

#include <algorithm>
#include <ranges>

using namespace State::Workspace;

static bool monitorExists(std::span<const SMonitorSnapshot> monitors, std::string_view address) {
    return std::ranges::any_of(monitors, [address](const auto& monitor) { return monitor.address == address; });
}

static std::optional<SWorkspaceSnapshot> findWorkspace(std::span<const SWorkspaceSnapshot> workspaces, const SWorkspaceIdentity& identity) {
    const auto WORKSPACE = std::ranges::find(workspaces, identity, &SWorkspaceSnapshot::identity);
    return WORKSPACE == workspaces.end() ? std::nullopt : std::optional{*WORKSPACE};
}

std::optional<SWorkspaceIdentity> CMonitorLifecyclePolicy::selectDefaultWorkspace(std::string_view monitorAddress, const std::optional<SWorkspaceIdentity>& configuredDefault,
                                                                                  std::span<const SDefaultWorkspaceCandidate> candidates,
                                                                                  std::span<const SWorkspaceSnapshot>         workspaces) {
    if (configuredDefault && !configuredDefault->address.empty())
        return configuredDefault;

    for (const auto& candidate : candidates) {
        if (candidate.identity.address.empty())
            continue;

        if (candidate.boundMonitorAddress && *candidate.boundMonitorAddress != monitorAddress)
            continue;

        if (findWorkspace(workspaces, candidate.identity))
            continue;

        return candidate.identity;
    }

    return std::nullopt;
}

void CMonitorLifecyclePolicy::monitorConnected(const SMonitorSnapshot& monitor, IPolicyContext& context) {
    if (monitor.address.empty())
        return;

    const auto MONITORS   = context.monitors();
    const auto REAL_COUNT = std::ranges::count_if(MONITORS, [](const auto& candidate) { return !candidate.fallback; });
    auto       workspaces = context.workspaces();

    for (const auto& workspace : workspaces) {
        const auto RETURN    = std::ranges::find_if(m_returnMonitors, [&workspace](const auto& entry) { return entry.first == workspace.identity; });
        const bool RETURNING = RETURN != m_returnMonitors.end() && RETURN->second == monitor.address;
        const bool ORPHANED  = !workspace.monitorAddress || !monitorExists(MONITORS, *workspace.monitorAddress);
        const bool RECOVERY  = ORPHANED && (monitor.fallback || REAL_COUNT == 1);

        if (!RETURNING && !RECOVERY)
            continue;

        if (workspace.monitorAddress != monitor.address)
            context.moveWorkspace(workspace.identity, monitor.address);

        if (RETURNING)
            std::erase_if(m_returnMonitors, [&](const auto& entry) { return entry.first == workspace.identity && entry.second == monitor.address; });
    }

    if (const auto REMEMBERED = rememberedActiveWorkspace(monitor.address); REMEMBERED) {
        workspaces                  = context.workspaces();
        const auto ACTIVE_WORKSPACE = findWorkspace(workspaces, *REMEMBERED);
        if (ACTIVE_WORKSPACE) {
            if (ACTIVE_WORKSPACE->monitorAddress != monitor.address)
                context.moveWorkspace(*REMEMBERED, monitor.address);

            context.activateWorkspace(*REMEMBERED, monitor.address);
            return;
        }
    }

    const auto UPDATED_MONITORS = context.monitors();
    const auto UPDATED_MONITOR  = std::ranges::find(UPDATED_MONITORS, monitor.address, &SMonitorSnapshot::address);
    if (UPDATED_MONITOR != UPDATED_MONITORS.end() && UPDATED_MONITOR->activeWorkspace)
        return;

    workspaces         = context.workspaces();
    const auto DEFAULT = selectDefaultWorkspace(monitor.address, monitor.fallback ? std::nullopt : context.configuredDefaultWorkspace(monitor.address),
                                                context.defaultWorkspaceCandidates(monitor.address), workspaces);
    if (!DEFAULT)
        return;

    const auto WORKSPACE = findWorkspace(workspaces, *DEFAULT);
    if (!WORKSPACE)
        context.createWorkspace(*DEFAULT, monitor.address);
    else if (WORKSPACE->monitorAddress != monitor.address)
        context.moveWorkspace(*DEFAULT, monitor.address);

    context.activateWorkspace(*DEFAULT, monitor.address);
}

void CMonitorLifecyclePolicy::monitorDisconnected(const SMonitorSnapshot& monitor, IPolicyContext& context) {
    if (monitor.address.empty())
        return;

    if (monitor.activeWorkspace && !monitor.activeWorkspace->address.empty())
        m_rememberedActiveWorkspaces.insert_or_assign(monitor.address, *monitor.activeWorkspace);

    const auto MONITORS   = context.monitors();
    const auto BACKUP     = std::ranges::find_if(MONITORS, [&monitor](const auto& candidate) { return candidate.address != monitor.address; });
    const auto WORKSPACES = context.workspaces();

    for (const auto& workspace : WORKSPACES) {
        const bool ON_DISCONNECTING_MONITOR = workspace.monitorAddress == monitor.address;
        const bool ORPHANED                 = !workspace.monitorAddress || !monitorExists(MONITORS, *workspace.monitorAddress);

        if (!ON_DISCONNECTING_MONITOR && !ORPHANED)
            continue;

        const auto RETURN = std::ranges::find_if(m_returnMonitors, [&workspace](const auto& entry) { return entry.first == workspace.identity; });
        if (ON_DISCONNECTING_MONITOR && !monitor.fallback && RETURN == m_returnMonitors.end())
            m_returnMonitors.emplace_back(workspace.identity, monitor.address);

        if (BACKUP != MONITORS.end())
            context.moveWorkspace(workspace.identity, BACKUP->address);
    }
}

void CMonitorLifecyclePolicy::workspaceDestroyed(const SWorkspaceIdentity& identity) {
    std::erase_if(m_returnMonitors, [&identity](const auto& entry) { return entry.first == identity; });

    std::erase_if(m_rememberedActiveWorkspaces, [&identity](const auto& entry) { return entry.second == identity; });
}

void CMonitorLifecyclePolicy::workspaceIdentityChanged(const SWorkspaceIdentity& oldIdentity, const SWorkspaceIdentity& newIdentity) {
    if (oldIdentity == newIdentity)
        return;

    const auto RETURN = std::ranges::find_if(m_returnMonitors, [&oldIdentity](const auto& entry) { return entry.first == oldIdentity; });
    if (RETURN != m_returnMonitors.end())
        RETURN->first = newIdentity;

    for (auto& [monitor, workspace] : m_rememberedActiveWorkspaces) {
        if (workspace == oldIdentity)
            workspace = newIdentity;
    }
}

void CMonitorLifecyclePolicy::clear() {
    m_rememberedActiveWorkspaces.clear();
    m_returnMonitors.clear();
}

std::optional<SWorkspaceIdentity> CMonitorLifecyclePolicy::rememberedActiveWorkspace(std::string_view monitorAddress) const {
    const auto REMEMBERED = m_rememberedActiveWorkspaces.find(std::string{monitorAddress});
    if (REMEMBERED == m_rememberedActiveWorkspaces.end())
        return std::nullopt;

    return REMEMBERED->second;
}

std::optional<std::string> CMonitorLifecyclePolicy::returnMonitorAddress(const SWorkspaceIdentity& identity) const {
    const auto MONITOR = std::ranges::find_if(m_returnMonitors, [&identity](const auto& entry) { return entry.first == identity; });
    if (MONITOR == m_returnMonitors.end())
        return std::nullopt;

    return MONITOR->second;
}
