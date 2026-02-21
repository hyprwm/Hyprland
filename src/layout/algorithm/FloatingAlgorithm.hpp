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

        virtual void recenter(SP<ITarget> t);

        virtual void recalculate();

      protected:
        IFloatingAlgorithm() = default;

        WP<CAlgorithm> m_parent;

        friend class Layout::CAlgorithm;
    };
}