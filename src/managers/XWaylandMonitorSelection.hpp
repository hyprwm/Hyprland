#pragma once

#include "../helpers/Monitor.hpp"
#include "../SharedDefs.hpp"
#include <optional>
#include <span>

struct SXWaylandMonitorDesc {
    Vector2D waylandPosition;
    Vector2D xwaylandPosition;
    Vector2D size;
    Vector2D transformedSize;
    float    scale = 1.F;
};

std::optional<size_t> selectMonitorForWaylandPoint(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, bool forceZeroScaling);
std::optional<size_t> selectMonitorForXWaylandPoint(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, bool forceZeroScaling);

Vector2D              waylandToXWaylandCoords(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, bool forceZeroScaling,
                                              std::optional<size_t> preferred = {});
Vector2D              xwaylandToWaylandCoords(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, bool forceZeroScaling,
                                              std::optional<size_t> preferred = {});

std::optional<size_t> selectMonitorForWaylandPoint(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling);
std::optional<size_t> selectMonitorForXWaylandPoint(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling);

Vector2D              waylandToXWaylandCoords(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling, std::optional<size_t> preferred = {});
Vector2D              xwaylandToWaylandCoords(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling, std::optional<size_t> preferred = {});
