#pragma once

#include "../DesktopTypes.hpp"
#include "../../SharedDefs.hpp"
#include "../../macros.hpp"
#include "../../helpers/MiscFunctions.hpp"

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
            PHLWORKSPACEREF workspace;
            PHLMONITORREF   monitor;
            std::string     name = "";
            WORKSPACEID     id   = WORKSPACE_INVALID;
        };

        const SHistoryEntry previousWorkspace(PHLWORKSPACE ws);
        SWorkspaceIDName    previousWorkspaceIDName(PHLWORKSPACE ws);

        const SHistoryEntry previousWorkspace(PHLWORKSPACE ws, PHLMONITOR restrict);
        SWorkspaceIDName    previousWorkspaceIDName(PHLWORKSPACE ws, PHLMONITOR restrict);

      private:
        struct SLastWorkspaceData {
            PHLMONITORREF   monitor;
            PHLWORKSPACEREF workspace;
            std::string     workspaceName = "";
            WORKSPACEID     workspaceID   = WORKSPACE_INVALID;
        } m_lastWorkspaceData;

        std::deque<SHistoryEntry> m_history;

        void                     track(PHLWORKSPACE w);
        void                     gc();
        void                     setLastWorkspaceData(PHLWORKSPACE w);
    };

    SP<CWorkspaceHistoryTracker> workspaceTracker();
};
