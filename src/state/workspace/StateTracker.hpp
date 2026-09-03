#pragma once

#include "Query.hpp"

#include <vector>

namespace State::Workspace {
    class IStateTracker {
      public:
        virtual ~IStateTracker() = default;

        virtual const std::vector<PHLWORKSPACEREF>& workspaceRefs() const = 0;

        CQuery                                      query() const;
        bool                                        contains(PHLWORKSPACE workspace) const;

      protected:
        IStateTracker() = default;
    };
}
