#pragma once

#include "../helpers/math/Math.hpp"

#include <functional>

namespace Pointer {
    /*
     * A pointer transformer takes the absolute pointer coords and transforms them according to its own
     * logic. Pointer::mgr()->position() will return those coords.
     */
    class CPointerTransformer {
      public:
        CPointerTransformer(std::function<Vector2D(Vector2D)> transform);
        ~CPointerTransformer() = default;

        Vector2D transform(Vector2D pos) const;

      private:
        std::function<Vector2D(Vector2D)> m_transform;
    };
}
