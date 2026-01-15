#pragma once

#include "../DesktopTypes.hpp"
#include "../../SharedDefs.hpp"
#include "../../macros.hpp"
#include "../../helpers/MiscFunctions.hpp"

#include <vector>

namespace Desktop::History {
    class CWorkspaceHistoryTracker {
      public:
        CWorkspaceHistoryTracker();
        ~CWorkspaceHistoryTracker() = default;

        CWorkspaceHistoryTracker(const CWorkspaceHistoryTracker&) = delete;
        CWorkspaceHistoryTracker(CWorkspaceHistoryTracker&)       = delete;
        CWorkspaceHistoryTracker(CWorkspaceHistoryTracker&&)      = delete;

        struct SWorkspacePreviousData {
            PHLWORKSPACEREF workspace;
            PHLWORKSPACEREF previous;
            PHLMONITORREF   previousMon;
            std::string     previousName = "";
            WORKSPACEID     previousID   = WORKSPACE_INVALID;
        };

        const SWorkspacePreviousData* previousWorkspace(PHLWORKSPACE ws);
        SWorkspaceIDName              previousWorkspaceIDName(PHLWORKSPACE ws);

        const SWorkspacePreviousData* previousWorkspace(PHLWORKSPACE ws, PHLMONITOR restrict);
        SWorkspaceIDName              previousWorkspaceIDName(PHLWORKSPACE ws, PHLMONITOR restrict);

      private:
        struct SLastWorkspaceData {
            PHLMONITORREF   monitor;
            PHLWORKSPACEREF workspace;
            std::string     workspaceName = "";
            WORKSPACEID     workspaceID   = WORKSPACE_INVALID;
        } m_lastWorkspaceData;

        std::vector<SWorkspacePreviousData> m_datas;

        void                                track(PHLWORKSPACE w);
        void                                gc();
        void                                setLastWorkspaceData(PHLWORKSPACE w);

        SWorkspacePreviousData&             dataFor(PHLWORKSPACE ws);
    };

    SP<CWorkspaceHistoryTracker> workspaceTracker();
};