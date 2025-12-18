#include "WindowHistoryTracker.hpp"

#include "../../managers/HookSystemManager.hpp"
#include "../view/Window.hpp"

using namespace Desktop;
using namespace Desktop::History;

SP<CWindowHistoryTracker> History::windowTracker() {
    static SP<CWindowHistoryTracker> tracker = makeShared<CWindowHistoryTracker>();
    return tracker;
}

CWindowHistoryTracker::CWindowHistoryTracker() {
    static auto P = g_pHookSystem->hookDynamic("openWindowEarly", [this](void* self, SCallbackInfo& info, std::any data) {
        auto window = std::any_cast<PHLWINDOW>(data);

        // add a last track
        m_history.insert(m_history.begin(), window);
    });

    static auto P1 = g_pHookSystem->hookDynamic("activeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        auto window = std::any_cast<PHLWINDOW>(data);

        track(window);
    });
}

void CWindowHistoryTracker::track(PHLWINDOW w) {
    std::erase(m_history, w);
    m_history.emplace_back(w);
}

const std::vector<PHLWINDOWREF>& CWindowHistoryTracker::fullHistory() {
    gc();
    return m_history;
}

std::vector<PHLWINDOWREF> CWindowHistoryTracker::historyForWorkspace(PHLWORKSPACE ws) {
    gc();
    std::vector<PHLWINDOWREF> windows;

    for (const auto& w : m_history) {
        if (w->m_workspace != ws)
            continue;

        windows.emplace_back(w);
    }

    return windows;
}

void CWindowHistoryTracker::gc() {
    std::erase_if(m_history, [](const auto& e) { return !e; });
}
