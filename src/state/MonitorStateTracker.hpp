#pragma once

#include <vector>

#include "../desktop/DesktopTypes.hpp"
#include "MonitorQuery.hpp"

namespace State {
    class IMonitorStateTracker {
      public:
        virtual ~IMonitorStateTracker() = default;

        virtual const std::vector<PHLMONITOR>& allMonitors() const = 0;
        virtual const std::vector<PHLMONITOR>& monitors() const    = 0;

        virtual CMonitorQuery                  query() const;
        virtual bool                           contains(PHLMONITOR monitor) const;

      protected:
        IMonitorStateTracker() = default;
    };
}
