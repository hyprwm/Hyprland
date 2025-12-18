#pragma once

#include "../DesktopTypes.hpp"

#include <vector>

namespace Desktop::History {
    class CWindowHistoryTracker {
      public:
        CWindowHistoryTracker();
        ~CWindowHistoryTracker() = default;

        CWindowHistoryTracker(const CWindowHistoryTracker&) = delete;
        CWindowHistoryTracker(CWindowHistoryTracker&)       = delete;
        CWindowHistoryTracker(CWindowHistoryTracker&&)      = delete;

        // History is ordered old -> new, meaning .front() is oldest, while .back() is newest

        const std::vector<PHLWINDOWREF>& fullHistory();
        std::vector<PHLWINDOWREF>        historyForWorkspace(PHLWORKSPACE ws);

      private:
        std::vector<PHLWINDOWREF> m_history;

        void                      track(PHLWINDOW w);
        void                      gc();
    };

    SP<CWindowHistoryTracker> windowTracker();
};