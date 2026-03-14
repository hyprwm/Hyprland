#pragma once

#include "../helpers/Monitor.hpp"
#include "../SharedDefs.hpp"
#include <optional>
#include <span>

namespace XWayland {
    std::optional<size_t> selectMonitorForWaylandPoint(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling);
    std::optional<size_t> selectMonitorForXWaylandPoint(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling);

    Vector2D              waylandToXWaylandCoords(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling,
                                                  std::optional<size_t> preferred = {});
    Vector2D              xwaylandToWaylandCoords(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling,
                                                  std::optional<size_t> preferred = {});

    Vector2D              waylandToXWaylandCoords(const Vector2D& point, PHLMONITOR preferred = nullptr);
    Vector2D              xwaylandToWaylandCoords(const Vector2D& point, PHLMONITOR preferred = nullptr);
}
