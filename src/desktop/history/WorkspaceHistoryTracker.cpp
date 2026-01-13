#include "WorkspaceHistoryTracker.hpp"

#include "../../helpers/Monitor.hpp"
#include "../Workspace.hpp"
#include "../state/FocusState.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../config/ConfigValue.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

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

    static auto P1 = g_pHookSystem->hookDynamic("focusedMon", [this](void* self, SCallbackInfo& info, std::any data) {
        auto mon = std::any_cast<PHLMONITOR>(data);

        // This sucks ASS, but we have to do this because switching to a workspace on another mon will trigger a workspace event right afterwards and we don't
        // want to remember the workspace that was not visible there
        // TODO: do something about this
        g_pEventLoopManager->doLater([this, mon = PHLMONITORREF{mon}] {
            if (mon)
                track(mon->m_activeWorkspace);
        });
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
    if (!w || !w->m_monitor || w == m_lastWorkspaceData.workspace)
        return;

    static auto                   PALLOWWORKSPACECYCLES = CConfigValue<Hyprlang::INT>("binds:allow_workspace_cycles");

    auto&                         data = dataFor(w);

    Hyprutils::Utils::CScopeGuard x([&] { setLastWorkspaceData(w); });

    if (m_lastWorkspaceData.workspace == w && !*PALLOWWORKSPACECYCLES)
        return;

    data.previous = m_lastWorkspaceData.workspace;
    if (m_lastWorkspaceData.workspace) {
        data.previousName = m_lastWorkspaceData.workspace->m_name;
        data.previousID   = m_lastWorkspaceData.workspace->m_id;
        data.previousMon  = m_lastWorkspaceData.workspace->m_monitor;
    } else {
        data.previousName = m_lastWorkspaceData.workspaceName;
        data.previousID   = m_lastWorkspaceData.workspaceID;
        data.previousMon  = m_lastWorkspaceData.monitor;
    }
}

void CWorkspaceHistoryTracker::gc() {
    std::erase_if(m_datas, [](const auto& e) { return !e.workspace; });
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
