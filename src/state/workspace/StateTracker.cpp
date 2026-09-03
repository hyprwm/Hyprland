#include "StateTracker.hpp"

#include <algorithm>

using namespace State::Workspace;

CQuery IStateTracker::query() const {
    return CQuery{*this};
}

bool IStateTracker::contains(PHLWORKSPACE workspace) const {
    return std::ranges::any_of(workspaceRefs(), [&](const auto& candidate) { return candidate == workspace; });
}
