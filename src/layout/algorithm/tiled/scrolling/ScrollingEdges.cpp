#include "ScrollingEdges.hpp"

using namespace Layout;
using namespace Layout::Tiled;

STargetEdges Layout::Tiled::scrollingTargetEdges(size_t columnIndex, size_t columnCount, size_t targetIndex, size_t targetCount, bool primaryHorizontal, bool reversed) {
    if (columnIndex >= columnCount || targetIndex >= targetCount)
        return {};

    const auto COLUMN_BEFORE = columnIndex > 0 ? eTargetEdge::INNER : eTargetEdge::OUTER;
    const auto COLUMN_AFTER  = columnIndex + 1 < columnCount ? eTargetEdge::INNER : eTargetEdge::OUTER;
    const auto TARGET_BEFORE = targetIndex > 0 ? eTargetEdge::INNER : eTargetEdge::OUTER;
    const auto TARGET_AFTER  = targetIndex + 1 < targetCount ? eTargetEdge::INNER : eTargetEdge::OUTER;

    // calculateStripBox reverses the primary axis; calculateTargetBox never reverses the secondary axis.
    const auto PRIMARY_START = reversed ? COLUMN_AFTER : COLUMN_BEFORE;
    const auto PRIMARY_END   = reversed ? COLUMN_BEFORE : COLUMN_AFTER;

    if (primaryHorizontal)
        return {
            .top    = TARGET_BEFORE,
            .right  = PRIMARY_END,
            .bottom = TARGET_AFTER,
            .left   = PRIMARY_START,
        };

    return {
        .top    = PRIMARY_START,
        .right  = TARGET_AFTER,
        .bottom = PRIMARY_END,
        .left   = TARGET_BEFORE,
    };
}
