#pragma once

#include "workspace/State.hpp"

namespace State {
    using CWorkspaceStateTracker = Workspace::CState;

    inline UP<CWorkspaceStateTracker>& workspaceState() {
        return Workspace::state();
    }
}
