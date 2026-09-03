#pragma once

#include "workspace/PlacementController.hpp"

namespace State {
    using CWorkspacePlacementController = Workspace::CPlacementController;

    inline UP<CWorkspacePlacementController>& workspacePlacementController() {
        return Workspace::placementController();
    }
}
