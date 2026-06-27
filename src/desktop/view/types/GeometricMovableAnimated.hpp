#pragma once

#include "GeometricMovable.hpp"
#include "../../../helpers/AnimatedVariable.hpp"

namespace Desktop::View {
    class CGeometricMovableAnimated : public virtual IGeometricMovable {
      public:
        virtual ~CGeometricMovableAnimated() = default;

        virtual Vector2D position(eGeometricValueType) const override;
        virtual Vector2D size(eGeometricValueType) const override;
        virtual CBox     geometricBox(eGeometricValueType) const override;

        virtual void     move(const Vector2D& x) override;
        virtual void     resize(const Vector2D& x) override;
        virtual void     setBox(const CBox& x) override;

        virtual void     finishAnimation();

      protected:
        CGeometricMovableAnimated() = default;

        // needs to be exposed so that the child can set up the animations
        // TODO: don't?
        PHLANIMVAR<Vector2D> m_realPosition;
        PHLANIMVAR<Vector2D> m_realSize;
    };
};
