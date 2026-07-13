#pragma once

#include "Geometric.hpp"

namespace Desktop::View {
    class IGeometricMovable : public virtual IGeometric {
      public:
        virtual ~IGeometricMovable() = default;

        virtual void move(const Vector2D& x)   = 0;
        virtual void resize(const Vector2D& x) = 0;
        virtual void setBox(const CBox& x)     = 0;

      protected:
        IGeometricMovable() = default;
    };
};
