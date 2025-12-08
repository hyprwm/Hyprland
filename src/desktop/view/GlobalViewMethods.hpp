#pragma once

#include "View.hpp"

#include "../Workspace.hpp"

#include <vector>

namespace Desktop::View {
    std::vector<SP<IView>> getViewsForWorkspace(PHLWORKSPACE ws);
};