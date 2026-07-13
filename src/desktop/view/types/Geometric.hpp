#pragma once

#include "../../../helpers/math/Math.hpp"
#include "../../../helpers/memory/Memory.hpp"

#include <vector>
#include <cstdint>

namespace Desktop::View {
    class IGeometric {
      public:
        enum eGeometricValueType : uint8_t {
            GEOMETRIC_CURRENT = 0,
            GEOMETRIC_GOAL,
        };

        virtual ~IGeometric() = default;

        virtual Vector2D                    position(eGeometricValueType) const     = 0;
        virtual Vector2D                    size(eGeometricValueType) const         = 0;
        virtual CBox                        geometricBox(eGeometricValueType) const = 0;

        virtual std::vector<SP<IGeometric>> geometricChildren() const;

      protected:
        IGeometric() = default;
    };
};
