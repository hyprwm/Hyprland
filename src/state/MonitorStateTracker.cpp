#include "MonitorStateTracker.hpp"
#include "MonitorQuery.hpp"

#include <algorithm>

using namespace State;

CMonitorQuery IMonitorStateTracker::query() const {
    return CMonitorQuery{*this};
}

bool IMonitorStateTracker::contains(PHLMONITOR monitor) const {
    return std::ranges::contains(allMonitors(), monitor);
}
