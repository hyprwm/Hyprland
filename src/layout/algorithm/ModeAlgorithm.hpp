#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/math/Direction.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "../LayoutManager.hpp"

#include <expected>

namespace Layout {

    class ITarget;
    class CAlgorithm;

    class IModeAlgorithm {
      public:
        virtual ~IModeAlgorithm() = default;

        // a completely new target
        virtual void newTarget(SP<ITarget> target) = 0;

        // a target moved into the algorithm (from another)
        virtual void movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt) = 0;

        // a target removed
        virtual void removeTarget(SP<ITarget> target) = 0;

        // a target is being resized by a delta. Corner none likely means not interactive
        virtual void resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE) = 0;

        // recalculate layout
        virtual void recalculate() = 0;

        // swap targets
        virtual void swapTargets(SP<ITarget> a, SP<ITarget> b) = 0;

        // move a target in a given direction
        virtual void moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) = 0;

        // optional: handle layout messages
        virtual std::expected<void, std::string> layoutMsg(const std::string_view& sv);

        // optional: predict new window's size
        virtual std::optional<Vector2D> predictSizeForNewTarget();

      protected:
        IModeAlgorithm() = default;

        WP<CAlgorithm> m_parent;

        friend class Layout::CAlgorithm;
    };
}