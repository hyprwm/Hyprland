#pragma once

#include "../helpers/Monitor.hpp"
#include "../SharedDefs.hpp"
#include <optional>
#include <span>

namespace XWayland {
    template <typename MonitorT, typename MatchFn>
    std::optional<size_t> preferredMonitorIndex(std::span<const MonitorT> monitors, const std::optional<MonitorT>& preferred, MatchFn matches) {
        if (!preferred)
            return std::nullopt;

        for (size_t i = 0; i < monitors.size(); ++i) {
            if (matches(monitors[i], *preferred))
                return i;
        }

        return std::nullopt;
    }

    std::optional<size_t> selectMonitorForWaylandPoint(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling);
    std::optional<size_t> selectMonitorForXWaylandPoint(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling);

    Vector2D              waylandToXWaylandCoords(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling,
                                                  std::optional<size_t> preferred = {});
    Vector2D              xwaylandToWaylandCoords(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling,
                                                  std::optional<size_t> preferred = {});

}
