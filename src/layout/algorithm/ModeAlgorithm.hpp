#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "../LayoutManager.hpp"

namespace Layout {

    class ITarget;
    class CAlgorithm;

    class IModeAlgorithm {
      public:
        virtual ~IModeAlgorithm() = default;

        // a completely new target
        virtual void newTarget(SP<ITarget> target) = 0;

        // a target moved into the algorithm (from another)
        virtual void movedTarget(SP<ITarget> target) = 0;

        // a target removed
        virtual void removeTarget(SP<ITarget> target) = 0;

        // a target is being resized by a delta. Corner none likely means not interactive
        virtual void resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE) = 0;

        // recalculate layout
        virtual void recalculate() = 0;

      protected:
        IModeAlgorithm() = default;

        WP<CAlgorithm> m_parent;

        friend class Layout::CAlgorithm;
    };
}