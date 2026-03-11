#include "XWaylandMonitorSelection.hpp"

#include "../helpers/MiscFunctions.hpp"

static Vector2D effectiveMonitorSize(const SXWaylandMonitorDesc& monitor, bool forceZeroScaling) {
    return forceZeroScaling ? monitor.transformedSize : monitor.size;
}

static bool containsPoint(const Vector2D& point, const Vector2D& position, const Vector2D& size) {
    return point.x >= position.x && point.x < position.x + size.x && point.y >= position.y && point.y < position.y + size.y;
}

static float distanceToRect(const Vector2D& point, const Vector2D& position, const Vector2D& size) {
    return vecToRectDistanceSquared(point, position, position + size - Vector2D{1, 1});
}

static float distanceToRectCenter(const Vector2D& point, const Vector2D& position, const Vector2D& size) {
    const auto center = position + size / 2.F;
    const auto delta  = point - center;
    return delta.x * delta.x + delta.y * delta.y;
}

template <typename DistPosFn, typename DistSizeFn>
static std::optional<size_t> selectMonitorGeneric(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, DistPosFn distPos, DistSizeFn distSize) {
    std::optional<size_t> bestMonitor;
    auto                  bestDist   = std::numeric_limits<float>::max();
    auto                  bestCenter = std::numeric_limits<float>::max();

    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto dist   = distanceToRect(point, distPos(monitors[i]), distSize(monitors[i]));
        const auto center = distanceToRectCenter(point, distPos(monitors[i]), distSize(monitors[i]));

        if (dist < bestDist || (dist == bestDist && center < bestCenter)) {
            bestMonitor = i;
            bestDist    = dist;
            bestCenter  = center;
        }
    }

    return bestMonitor;
}

std::optional<size_t> selectMonitorForWaylandPoint(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, bool forceZeroScaling) {
    return selectMonitorGeneric(monitors, point, [](const SXWaylandMonitorDesc& monitor) { return monitor.waylandPosition; },
                                [&](const SXWaylandMonitorDesc& monitor) { return effectiveMonitorSize(monitor, forceZeroScaling); });
}

std::optional<size_t> selectMonitorForXWaylandPoint(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, bool forceZeroScaling) {
    return selectMonitorGeneric(monitors, point, [](const SXWaylandMonitorDesc& monitor) { return monitor.xwaylandPosition; },
                                [&](const SXWaylandMonitorDesc& monitor) { return effectiveMonitorSize(monitor, forceZeroScaling); });
}

Vector2D waylandToXWaylandCoords(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, bool forceZeroScaling, std::optional<size_t> preferred) {
    const auto monitorIndex = preferred ? preferred : selectMonitorForWaylandPoint(monitors, point, forceZeroScaling);
    if (!monitorIndex)
        return {};

    const auto& monitor = monitors[*monitorIndex];
    auto        result  = point - monitor.waylandPosition;

    if (forceZeroScaling)
        result *= monitor.scale;

    result += monitor.xwaylandPosition;
    return result;
}

Vector2D xwaylandToWaylandCoords(std::span<const SXWaylandMonitorDesc> monitors, const Vector2D& point, bool forceZeroScaling, std::optional<size_t> preferred) {
    const auto monitorIndex = preferred ? preferred : selectMonitorForXWaylandPoint(monitors, point, forceZeroScaling);
    if (!monitorIndex)
        return {};

    const auto& monitor = monitors[*monitorIndex];
    auto        result  = point - monitor.xwaylandPosition;

    if (forceZeroScaling)
        result /= monitor.scale;

    result += monitor.waylandPosition;
    return result;
}
