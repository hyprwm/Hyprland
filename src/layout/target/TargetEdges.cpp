#include "TargetEdges.hpp"

std::array<Layout::STargetEdges, 2> Layout::splitTargetEdges(const STargetEdges& edges, bool sideBySide) {
    std::array<STargetEdges, 2> children = {
        edges,
        edges,
    };

    if (sideBySide) {
        children[0].right = eTargetEdge::INNER;
        children[1].left  = eTargetEdge::INNER;
    } else {
        children[0].bottom = eTargetEdge::INNER;
        children[1].top    = eTargetEdge::INNER;
    }

    return children;
}
