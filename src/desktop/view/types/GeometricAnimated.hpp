#pragma once

#include "Geometric.hpp"
#include "../../../helpers/AnimatedVariable.hpp"

namespace Desktop::View {
    class CGeometricAnimated : public virtual IGeometric {
      public:
        virtual ~CGeometricAnimated() = default;

        virtual Vector2D position(eGeometricValueType) const override;
        virtual Vector2D size(eGeometricValueType) const override;
        virtual CBox     geometricBox(eGeometricValueType) const override;

        virtual void     finishAnimation();

      protected:
        CGeometricAnimated() = default;

        // managed by the child
        PHLANIMVAR<Vector2D> m_realPosition;
        PHLANIMVAR<Vector2D> m_realSize;
    };
};
