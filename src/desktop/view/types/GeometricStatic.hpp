#pragma once

#include "Geometric.hpp"

namespace Desktop::View {
    class CGeometricStatic : public virtual IGeometric {
      public:
        virtual ~CGeometricStatic() = default;

        virtual Vector2D position(eGeometricValueType) const override;
        virtual Vector2D size(eGeometricValueType) const override;
        virtual CBox     geometricBox(eGeometricValueType) const override;

      protected:
        CGeometricStatic() = default;

        // non-movable, object manages its own box itself
        CBox m_box = {};
    };
};
