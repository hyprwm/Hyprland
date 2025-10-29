#pragma once

#include "../DesktopTypes.hpp"
#include "../../helpers/math/Math.hpp"

namespace Interactive {
    class CDrag;

    enum eWindowDragMode : int8_t {
        WINDOW_DRAG_INVALID            = -1,
        WINDOW_DRAG_MOVE               = 0,
        WINDOW_DRAG_RESIZE             = 1,
        WINDOW_DRAG_RESIZE_BLOCK_RATIO = 2,
        WINDOW_DRAG_RESIZE_FORCE_RATIO = 3
    };

    enum eRectCorner : uint8_t {
        CORNER_NONE        = 0,
        CORNER_TOPLEFT     = (1 << 0),
        CORNER_TOPRIGHT    = (1 << 1),
        CORNER_BOTTOMRIGHT = (1 << 2),
        CORNER_BOTTOMLEFT  = (1 << 3),
    };

    inline eRectCorner cornerFromBox(const CBox& box, const Vector2D& pos) {
        const auto CENTER = box.middle();

        if (pos.x < CENTER.x)
            return pos.y < CENTER.y ? CORNER_TOPLEFT : CORNER_BOTTOMLEFT;
        return pos.y < CENTER.y ? CORNER_TOPRIGHT : CORNER_BOTTOMRIGHT;
    }
};