#pragma once

#include "../../helpers/math/Math.hpp"

namespace Animation {
    template <typename T>
    struct SAnimatedMovement {
        T from = {};
        T to   = {};
    };

    struct SViewAnimationContext {
        SAnimatedMovement<Vector2D> pos, size;
        SAnimatedMovement<float>    alpha;
    };

    class IViewAnimationController {
      public:
        virtual ~IViewAnimationController() = default;

        virtual SViewAnimationContext animateIn() const  = 0;
        virtual SViewAnimationContext animateOut() const = 0;
    };
};
