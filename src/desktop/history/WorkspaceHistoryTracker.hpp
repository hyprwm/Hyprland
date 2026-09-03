#pragma once

#include "../DesktopTypes.hpp"
#include "../../SharedDefs.hpp"
#include "../../macros.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include "../../state/workspace/Target.hpp"

#include <deque>

namespace Desktop::History {
    class CWorkspaceHistoryTracker {
      public:
        CWorkspaceHistoryTracker();
        ~CWorkspaceHistoryTracker() = default;

        CWorkspaceHistoryTracker(const CWorkspaceHistoryTracker&) = delete;
        CWorkspaceHistoryTracker(CWorkspaceHistoryTracker&)       = delete;
        CWorkspaceHistoryTracker(CWorkspaceHistoryTracker&&)      = delete;

        struct SHistoryEntry {
            PHLWORKSPACEREF           workspace;
            PHLMONITORREF             monitor;
            State::Workspace::STarget target;
        };

        const SHistoryEntry previousWorkspace(PHLWORKSPACE ws);

        const SHistoryEntry previousWorkspace(PHLWORKSPACE ws, PHLMONITOR restrict);
        void                workspaceIdentityChanged(PHLWORKSPACE workspace);

      private:
        std::deque<SHistoryEntry> m_history;

        void                      track(PHLWORKSPACE w);
        void                      gc();
    };

    SP<CWorkspaceHistoryTracker> workspaceTracker();
};
