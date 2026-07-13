#pragma once

#include "IMonitorIdentifiable.hpp"
#include "IMonitorMutableGeometry.hpp"

#include <cstdint>
#include <optional>

namespace Config {
    enum eAutoDirs : uint8_t;
}

namespace Monitor {
    class IMonitorArrangeable : public virtual IMonitorIdentifiable, public virtual IMonitorMutableGeometry {
      public:
        virtual std::optional<Vector2D> explicitPosition() const                 = 0;
        virtual Config::eAutoDirs       autoDirection() const                    = 0;
        virtual Vector2D                xwaylandPosition() const                 = 0;
        virtual float                   xwaylandScale() const                    = 0;
        virtual void                    setXWaylandPosition(const Vector2D& pos) = 0;
        virtual void                    setXWaylandScale(float scale)            = 0;
    };
}
