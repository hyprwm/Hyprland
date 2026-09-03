#include "WorkspaceHistoryTracker.hpp"

#include "../../output/Monitor.hpp"
#include "../../workspace/HLWorkspace.hpp"
#include "../state/FocusState.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../event/EventBus.hpp"
#include "../../config/ConfigValue.hpp"

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
    m_history.push_front(SHistoryEntry{
        .workspace = ws,
        .monitor   = ws->m_monitor,
        .target    = {.id = ws->id(), .address = ws->addressableName(), .displayName = ws->displayName(), .type = ws->type()},
    });
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
    return {};
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
    return {};
}

void CWorkspaceHistoryTracker::workspaceIdentityChanged(PHLWORKSPACE workspace) {
    if (!workspace)
        return;

    for (auto& entry : m_history) {
        if (entry.workspace != workspace)
            continue;

        entry.target = {
            .id          = workspace->id(),
            .address     = workspace->addressableName(),
            .displayName = workspace->displayName(),
            .type        = workspace->type(),
        };
    }
}
