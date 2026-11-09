#pragma once

#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"

#include "ModeAlgorithm.hpp"

namespace Layout {

    class ITarget;
    class CAlgorithm;

    class IFloatingAlgorithm : public IModeAlgorithm {
      public:
        virtual ~IFloatingAlgorithm() = default;

        // a target is being moved by a delta
        virtual void moveTarget(const Vector2D& Δ, SP<ITarget> target) = 0;

        // a target is moved to a pos x size
        virtual void setTargetGeom(const CBox& geom, SP<ITarget> target) = 0;

        // Called after a target geometry change; implementations must not move the target.
        virtual void updateTarget(SP<ITarget> target);

        virtual void recenter(SP<ITarget> t);

        virtual void recalculate(eRecalculateReason reason = RECALCULATE_REASON_UNKNOWN);

      protected:
        IFloatingAlgorithm() = default;

        friend class Layout::CAlgorithm;
    };
}