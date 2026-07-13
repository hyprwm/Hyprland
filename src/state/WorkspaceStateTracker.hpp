#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "WorkspaceQuery.hpp"
#include "WorkspaceQueryCore.hpp"

#include <vector>

namespace State {
    class IWorkspaceStateTracker {
      public:
        virtual ~IWorkspaceStateTracker() = default;

        virtual const std::vector<PHLWORKSPACEREF>& workspaceRefs() const       = 0;
        virtual std::vector<SWorkspaceQueryable>    queryableWorkspaces() const = 0;

        virtual CWorkspaceQuery                     query() const;
        virtual bool                                contains(PHLWORKSPACE workspace) const;

      protected:
        IWorkspaceStateTracker() = default;
    };
}
