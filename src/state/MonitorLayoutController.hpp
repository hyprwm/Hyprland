#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../helpers/math/Math.hpp"
#include "../desktop/DesktopTypes.hpp"

namespace State {
    class CMonitorLayoutController {
      public:
        void                scheduleRecheck();
        void                arrange() const;
        void                checkOverlapsAndNotify() const;

        bool                isPointOnAnyMonitor(const Vector2D&);
        bool                isPointOnReservedArea(const Vector2D& point, const PHLMONITOR monitor = nullptr);
        std::optional<CBox> calculateUnifiedX11WorkArea() const;

        bool                isVRRActiveOnAnyMonitor() const;

      private:
        bool m_scheduled = false;
    };

    UP<CMonitorLayoutController>& monitorLayoutController();
}
