#include "WorkspaceHistoryTracker.hpp"

#include "../../helpers/Monitor.hpp"
#include "../Workspace.hpp"
#include "../state/FocusState.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../event/EventBus.hpp"
#include "../../config/ConfigValue.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

#include <ranges>

using namespace Desktop;
using namespace Desktop::History;

SP<CWorkspaceHistoryTracker> History::workspaceTracker() {
    static SP<CWorkspaceHistoryTracker> tracker = makeShared<CWorkspaceHistoryTracker>();
    return tracker;
}

CWorkspaceHistoryTracker::CWorkspaceHistoryTracker() {
    static auto P = Event::bus()->m_events.workspace.active.listen([this](PHLWORKSPACE workspace) { track(workspace); });

    static auto P1 = Event::bus()->m_events.monitor.focused.listen([this](PHLMONITOR mon) {
        // This sucks ASS, but we have to do this because switching to a workspace on another mon will trigger a workspace event right afterwards and we don't
        // want to remember the workspace that was not visible there
        // TODO: do something about this
        g_pEventLoopManager->doLater([this, mon = PHLMONITORREF{mon}] {
            if (mon)
                track(mon->m_activeWorkspace);
        });
    });
}

void CWorkspaceHistoryTracker::track(PHLWORKSPACE ws) {
    if (!ws || !ws->m_monitor)
        return;

    static auto PALLOWWORKSPACECYCLES = CConfigValue<Config::INTEGER>("binds:allow_workspace_cycles");

    if (!m_history.empty() && m_history.front().workspace == ws && !*PALLOWWORKSPACECYCLES)
        return;

    // Erase from timeline if it exists so we can move it to the very front
    std::erase_if(m_history, [&](const auto& entry) { return entry.workspace == ws; });

    // Push the newly focused workspace to the top of our MRU list
    m_history.push_front(SHistoryEntry{.workspace = ws, .monitor = ws->m_monitor, .name = ws->m_name, .id = ws->m_id});

    Hyprutils::Utils::CScopeGuard x([&] { setLastWorkspaceData(ws); });
}

void CWorkspaceHistoryTracker::gc() {
    std::vector<PHLMONITORREF> monitorCounts;
    std::erase_if(m_history, [&](const auto& entry) {
        // Search if the monitor has been seen already
        for (auto& mon : monitorCounts | std::views::drop(1)) {
            // Remove entry
            if (mon == entry.monitor)
                return !entry.workspace;
        }
        // Add monitor to seen monitors
        monitorCounts.emplace_back(entry.monitor);
        return false;
    });
}

const CWorkspaceHistoryTracker::SHistoryEntry CWorkspaceHistoryTracker::previousWorkspace(PHLWORKSPACE ws) {
    gc();
    auto it = std::ranges::find_if(m_history, [&](const auto& entry) { return entry.workspace == ws; });

    // If the workspace is found in history, the previous one is simply the next element down the timeline
    if (it != m_history.end() && std::next(it) != m_history.end())
        return *std::next(it);

    // No prior history found
    return SHistoryEntry{.id = WORKSPACE_INVALID};
}

SWorkspaceIDName CWorkspaceHistoryTracker::previousWorkspaceIDName(PHLWORKSPACE ws) {
    const auto DATA = previousWorkspace(ws);

    if (DATA.id == WORKSPACE_INVALID)
        return SWorkspaceIDName{.id = WORKSPACE_INVALID};

    return SWorkspaceIDName{.id = DATA.id, .name = DATA.name, .isAutoIDd = DATA.id <= 0};
}

const CWorkspaceHistoryTracker::SHistoryEntry CWorkspaceHistoryTracker::previousWorkspace(PHLWORKSPACE ws, PHLMONITOR restrict) {
    if (!restrict)
        return previousWorkspace(ws);

    gc();

    auto it = std::ranges::find_if(m_history, [&](const auto& entry) { return entry.workspace == ws; });

    // Start looking from the element immediately following `ws` in the list
    if (it != m_history.end())
        it++;
    else
        it = m_history.begin();

    // Scan down the timeline until we hit a workspace mapped to the restricted monitor
    while (it != m_history.end()) {
        if (it->monitor == restrict)
            return *it;

        it++;
    }

    // Entry not found
    return SHistoryEntry{.id = WORKSPACE_INVALID};
}

SWorkspaceIDName CWorkspaceHistoryTracker::previousWorkspaceIDName(PHLWORKSPACE ws, PHLMONITOR restrict) {
    const auto DATA = previousWorkspace(ws, restrict);
    if (DATA.id == WORKSPACE_INVALID)
        return SWorkspaceIDName{.id = WORKSPACE_INVALID};

    return SWorkspaceIDName{.id = DATA.id, .name = DATA.name, .isAutoIDd = DATA.id <= 0};
}

void CWorkspaceHistoryTracker::setLastWorkspaceData(PHLWORKSPACE w) {
    if (!w) {
        m_lastWorkspaceData = {};
        return;
    }

    m_lastWorkspaceData.workspace     = w;
    m_lastWorkspaceData.workspaceID   = w->m_id;
    m_lastWorkspaceData.workspaceName = w->m_name;
    m_lastWorkspaceData.monitor       = w->m_monitor;
}
