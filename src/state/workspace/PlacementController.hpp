#pragma once

#include "../../SharedDefs.hpp"
#include "../../desktop/DesktopTypes.hpp"
#include "../../config/shared/workspace/WorkspaceRule.hpp"
#include "../../helpers/memory/Memory.hpp"

#include <functional>
#include <vector>

namespace State::Workspace {
    class CPlacementController {
      public:
        using FMoveWorkspace = std::function<void(PHLWORKSPACE, PHLMONITOR, bool)>;

        void ensurePersistentWorkspacesPresent(PHLWORKSPACE pWorkspace, const FMoveWorkspace& moveWorkspace) const;
        void ensurePersistentWorkspacesPresent(const std::vector<SP<Config::CWorkspaceRule>>& rules, PHLWORKSPACE pWorkspace, const FMoveWorkspace& moveWorkspace) const;
        void ensureWorkspacesOnAssignedMonitors(const FMoveWorkspace& moveWorkspace) const;
        void moveWorkspaceToMonitor(PHLWORKSPACE, PHLMONITOR, bool noWarpCursor = false, bool carryFocus = true) const;
        void swapActiveWorkspaces(PHLMONITOR, PHLMONITOR) const;
    };

    UP<CPlacementController>& placementController();
}
