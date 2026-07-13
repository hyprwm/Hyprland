#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../helpers/math/Math.hpp"

namespace Pointer {
    class CPointerController {
      public:
        void warpTo(const Vector2D& point, bool force = false) const;
    };

    UP<CPointerController>& pointerController();
}