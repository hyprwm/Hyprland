#include "WorkspaceStateTracker.hpp"
#include "../desktop/Workspace.hpp"

#include <algorithm>

using namespace State;

CWorkspaceQuery IWorkspaceStateTracker::query() const {
    return CWorkspaceQuery{*this};
}

bool IWorkspaceStateTracker::contains(PHLWORKSPACE workspace) const {
    return std::ranges::any_of(workspaceRefs(), [&](const auto& w) { return w == workspace; });
}
