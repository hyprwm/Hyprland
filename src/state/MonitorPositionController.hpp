#pragma once

#include "../output/IMonitorArrangeable.hpp"
#include "../helpers/memory/Memory.hpp"

#include <span>
#include <vector>

namespace State {
    class CMonitorPositionController {
      public:
        void arrange(std::span<const SP<Monitor::IMonitorArrangeable>> monitors, bool xwaylandForceZeroScaling) const;
    };

    UP<CMonitorPositionController>& monitorPositionController();
}
