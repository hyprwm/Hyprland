#pragma once

#include "IMonitorGeometry.hpp"

namespace Monitor {
    class IMonitorMutableGeometry : public virtual IMonitorGeometry {
      public:
        virtual void moveTo(const Vector2D& pos) = 0;
    };
}
