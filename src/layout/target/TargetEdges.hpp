#pragma once

#include <cstdint>

namespace Layout {
    enum class eTargetEdge : uint8_t {
        OUTER,
        INNER,
    };

    // Layout topology only: INNER faces another tile, OUTER faces exposed space.
    struct STargetEdges {
        eTargetEdge top    = eTargetEdge::OUTER;
        eTargetEdge right  = eTargetEdge::OUTER;
        eTargetEdge bottom = eTargetEdge::OUTER;
        eTargetEdge left   = eTargetEdge::OUTER;

        bool        operator==(const STargetEdges&) const = default;
    };
}
