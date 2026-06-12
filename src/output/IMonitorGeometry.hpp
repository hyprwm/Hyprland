#pragma once

#include "../SharedDefs.hpp"

namespace Monitor {
    class IMonitorGeometry {
      public:
        virtual ~IMonitorGeometry() = default;

        virtual Vector2D                    position() const                = 0;
        virtual Vector2D                    size() const                    = 0;
        virtual Vector2D                    pixelSize() const               = 0;
        virtual Vector2D                    transformedSize() const         = 0;
        virtual float                       scale() const                   = 0;
        virtual Hyprutils::Math::eTransform transform() const               = 0;
        virtual CBox                        logicalBox() const              = 0;
        virtual CBox                        logicalBoxMinusReserved() const = 0;
        virtual Vector2D                    middle() const                  = 0;
    };
}
