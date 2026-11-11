#include "MasterEdges.hpp"

using namespace Layout;
using namespace Layout::Tiled;

// Center-master slaves alternate left/right columns starting on the fallback side, so an odd
// count puts the extra slave there: left by default, right when extraToRight. Allocation,
// resizing and edge classification share this split. Returns {left, right}.
std::pair<int, int> Layout::Tiled::centerSlaveColumns(int slaveCount, bool extraToRight) {
    const int LARGER  = (slaveCount + 1) / 2;
    const int SMALLER = slaveCount - LARGER;
    return extraToRight ? std::pair{SMALLER, LARGER} : std::pair{LARGER, SMALLER};
}

STargetEdges Layout::Tiled::masterTargetEdges(eOrientation orientation, bool isMaster, int index, int masters, int slaves, bool extraToRight) {
    STargetEdges edges;
    int          count = isMaster ? masters : slaves;

    if (orientation == ORIENTATION_CENTER) {
        const auto [LEFT, RIGHT] = centerSlaveColumns(slaves, extraToRight);
        if (isMaster) {
            if (LEFT > 0)
                edges.left = eTargetEdge::INNER;
            if (RIGHT > 0)
                edges.right = eTargetEdge::INNER;
        } else {
            const bool ONRIGHT = (index % 2 == 0) == extraToRight;
            count              = ONRIGHT ? RIGHT : LEFT;
            index /= 2;
            if (masters > 0) {
                if (ONRIGHT)
                    edges.left = eTargetEdge::INNER;
                else
                    edges.right = eTargetEdge::INNER;
            }
        }
    } else if ((isMaster ? slaves : masters) > 0) {
        switch (orientation) {
            case ORIENTATION_LEFT: (isMaster ? edges.right : edges.left) = eTargetEdge::INNER; break;
            case ORIENTATION_RIGHT: (isMaster ? edges.left : edges.right) = eTargetEdge::INNER; break;
            case ORIENTATION_TOP: (isMaster ? edges.bottom : edges.top) = eTargetEdge::INNER; break;
            case ORIENTATION_BOTTOM: (isMaster ? edges.top : edges.bottom) = eTargetEdge::INNER; break;
            case ORIENTATION_CENTER: break;
        }
    }

    const bool VERTICAL = orientation != ORIENTATION_TOP && orientation != ORIENTATION_BOTTOM;
    if (index > 0)
        (VERTICAL ? edges.top : edges.left) = eTargetEdge::INNER;
    if (index + 1 < count)
        (VERTICAL ? edges.bottom : edges.right) = eTargetEdge::INNER;

    return edges;
}
