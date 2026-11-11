#pragma once

#include "../../../target/TargetEdges.hpp"

#include <cstddef>

namespace Layout::Tiled {
    // Topology covers the entire tape, including targets outside the viewport.
    STargetEdges scrollingTargetEdges(size_t columnIndex, size_t columnCount, size_t targetIndex, size_t targetCount, bool primaryHorizontal, bool reversed);
}
