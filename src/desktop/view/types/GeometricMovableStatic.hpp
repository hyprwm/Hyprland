#pragma once

#include "GeometricMovable.hpp"

namespace Desktop::View {
    class CGeometricMovableStatic : public virtual IGeometricMovable {
      public:
        virtual ~CGeometricMovableStatic() = default;

        virtual Vector2D position(eGeometricValueType) const override;
        virtual Vector2D size(eGeometricValueType) const override;
        virtual CBox     geometricBox(eGeometricValueType) const override;

        virtual void     move(const Vector2D& x) override;
        virtual void     resize(const Vector2D& x) override;
        virtual void     setBox(const CBox& x) override;

      protected:
        CGeometricMovableStatic() = default;

      private:
        CBox m_box = {};
    };
};
