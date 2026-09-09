#pragma once

#include "View.hpp"

#include "../../workspace/HLWorkspace.hpp"

#include <vector>

namespace Desktop::View {
    std::vector<SP<IView>> getViewsForWorkspace(PHLWORKSPACE ws);
};
