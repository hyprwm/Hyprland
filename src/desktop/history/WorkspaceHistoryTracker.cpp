#include "WorkspaceHistoryTracker.hpp"

#include "../../helpers/Monitor.hpp"
#include "../Workspace.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../config/ConfigValue.hpp"

using namespace Desktop;
using namespace Desktop::History;

SP<CWorkspaceHistoryTracker> History::workspaceTracker() {
    static SP<CWorkspaceHistoryTracker> tracker = makeShared<CWorkspaceHistoryTracker>();
    return tracker;
}

CWorkspaceHistoryTracker::CWorkspaceHistoryTracker() {
    static auto P = g_pHookSystem->hookDynamic("workspace", [this](void* self, SCallbackInfo& info, std::any data) {
        auto workspace = std::any_cast<PHLWORKSPACE>(data);
        track(workspace);
    });

    static auto P1 = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any data) {
        auto mon = std::any_cast<PHLMONITOR>(data);
        track(mon);
    });
}

CWorkspaceHistoryTracker::SMonitorData& CWorkspaceHistoryTracker::dataFor(PHLMONITOR mon) {
    for (auto& ref : m_monitorDatas) {
        if (ref.monitor != mon)
            continue;

        return ref;
    }

    return m_monitorDatas.emplace_back(SMonitorData{
        .monitor = mon,
    });
}

CWorkspaceHistoryTracker::SWorkspacePreviousData& CWorkspaceHistoryTracker::dataFor(PHLWORKSPACE ws) {
    for (auto& ref : m_datas) {
        if (ref.workspace != ws)
            continue;

        return ref;
    }

    return m_datas.emplace_back(SWorkspacePreviousData{
        .workspace = ws,
    });
}

void CWorkspaceHistoryTracker::track(PHLWORKSPACE w) {
    if (!w->m_monitor)
        return;

    static auto PALLOWWORKSPACECYCLES = CConfigValue<Hyprlang::INT>("binds:allow_workspace_cycles");

    auto&       data    = dataFor(w);
    auto&       monData = dataFor(w->m_monitor.lock());

    if (!monData.workspace) {
        data.previous.reset();
        data.previousID   = WORKSPACE_INVALID;
        data.previousName = "";
        return;
    }

    if (monData.workspace == w && !*PALLOWWORKSPACECYCLES) {
        track(w->m_monitor.lock());
        return;
    }

    data.previous     = monData.workspace;
    data.previousName = monData.workspace->m_name;
    data.previousID   = monData.workspace->m_id;
    data.previousMon  = monData.workspace->m_monitor;

    track(w->m_monitor.lock());
}

void CWorkspaceHistoryTracker::track(PHLMONITOR mon) {
    auto& data         = dataFor(mon);
    data.workspace     = mon->m_activeWorkspace;
    data.workspaceName = mon->m_activeWorkspace ? mon->m_activeWorkspace->m_name : "";
    data.workspaceID   = mon->activeWorkspaceID();
}

void CWorkspaceHistoryTracker::gc() {
    std::erase_if(m_datas, [](const auto& e) { return !e.workspace; });
    std::erase_if(m_monitorDatas, [](const auto& e) { return !e.monitor; });
}

const CWorkspaceHistoryTracker::SWorkspacePreviousData* CWorkspaceHistoryTracker::previousWorkspace(PHLWORKSPACE ws) {
    gc();

    for (const auto& d : m_datas) {
        if (d.workspace != ws)
            continue;
        return &d;
    }

    return &dataFor(ws);
}

SWorkspaceIDName CWorkspaceHistoryTracker::previousWorkspaceIDName(PHLWORKSPACE ws) {
    gc();

    for (const auto& d : m_datas) {
        if (d.workspace != ws)
            continue;
        return SWorkspaceIDName{.id = d.previousID, .name = d.previousName, .isAutoIDd = d.previousID <= 0};
    }

    auto& d = dataFor(ws);
    return SWorkspaceIDName{.id = d.previousID, .name = d.previousName, .isAutoIDd = d.previousID <= 0};
}

const CWorkspaceHistoryTracker::SWorkspacePreviousData* CWorkspaceHistoryTracker::previousWorkspace(PHLWORKSPACE ws, PHLMONITOR restrict) {
    if (!restrict)
        return previousWorkspace(ws);

    auto& data = dataFor(ws);
    while (true) {

        // case 1: previous exists
        if (data.previous) {
            if (data.previous->m_monitor != restrict) {
                data = dataFor(data.previous.lock());
                continue;
            }

            break;
        }

        // case 2: previous doesnt exist, but we have mon
        if (data.previousMon) {
            if (data.previousMon != restrict)
                return nullptr;

            break;
        }

        // case 3: no mon and no previous
        return nullptr;
    }

    return &data;
}

SWorkspaceIDName CWorkspaceHistoryTracker::previousWorkspaceIDName(PHLWORKSPACE ws, PHLMONITOR restrict) {
    const auto DATA = previousWorkspace(ws, restrict);
    if (!DATA)
        return SWorkspaceIDName{.id = WORKSPACE_INVALID};

    return SWorkspaceIDName{.id = DATA->previousID, .name = DATA->previousName, .isAutoIDd = DATA->previousID <= 0};
}
