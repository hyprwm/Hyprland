#pragma once

#include "../../../target/TargetEdges.hpp"

#include <utility>

namespace Layout::Tiled {
    // Orientation determines which side of the screen the master area resides on.
    enum eOrientation : uint8_t {
        ORIENTATION_LEFT = 0,
        ORIENTATION_TOP,
        ORIENTATION_RIGHT,
        ORIENTATION_BOTTOM,
        ORIENTATION_CENTER,
    };

    std::pair<int, int> centerSlaveColumns(int slaveCount, bool extraToRight);

    // index is the allocation order within the master or slave group, before splitting centered slaves into columns.
    STargetEdges masterTargetEdges(eOrientation orientation, bool isMaster, int index, int masters, int slaves, bool extraToRight);
}
