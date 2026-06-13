#pragma once

#include "../SharedDefs.hpp"
#include "../config/shared/workspace/WorkspaceRule.hpp"
#include "../helpers/memory/Memory.hpp"

#include <functional>
#include <vector>

namespace State {
    class CWorkspacePlacementController {
      public:
        using FMoveWorkspace = std::function<void(PHLWORKSPACE, PHLMONITOR, bool)>;

        void ensurePersistentWorkspacesPresent(PHLWORKSPACE pWorkspace, const FMoveWorkspace& moveWorkspace) const;
        void ensurePersistentWorkspacesPresent(const std::vector<Config::CWorkspaceRule>& rules, PHLWORKSPACE pWorkspace, const FMoveWorkspace& moveWorkspace) const;
        void ensureWorkspacesOnAssignedMonitors(const FMoveWorkspace& moveWorkspace) const;
    };

    UP<CWorkspacePlacementController>& workspacePlacementController();
}
