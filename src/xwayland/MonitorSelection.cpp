#include "MonitorSelection.hpp"

#include "../helpers/MiscFunctions.hpp"

namespace XWayland {
    static Vector2D effectiveMonitorSize(const PHLMONITOR& monitor, bool forceZeroScaling) {
        return forceZeroScaling ? monitor->m_transformedSize : monitor->m_size;
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

    template <typename MonitorT, typename ValidFn, typename DistPosFn, typename DistSizeFn>
    static std::optional<size_t> selectMonitorGeneric(std::span<const MonitorT> monitors, const Vector2D& point, ValidFn valid, DistPosFn distPos, DistSizeFn distSize) {
        if (monitors.empty())
            return std::nullopt;

        if (monitors.size() == 1)
            return 0;

        std::optional<size_t> bestValidMonitor;
        auto                  bestValidDist   = std::numeric_limits<float>::max();
        auto                  bestValidCenter = std::numeric_limits<float>::max();
        size_t                validCount      = 0;

        std::optional<size_t> bestMonitor;
        auto                  bestDist   = std::numeric_limits<float>::max();
        auto                  bestCenter = std::numeric_limits<float>::max();

        for (size_t i = 0; i < monitors.size(); ++i) {
            const auto pos    = distPos(monitors[i]);
            const auto size   = distSize(monitors[i]);
            const auto dist   = distanceToRect(point, pos, size);
            const auto center = distanceToRectCenter(point, pos, size);

            if (valid(monitors[i])) {
                validCount++;

                if (!bestValidMonitor || dist < bestValidDist || (dist == bestValidDist && center < bestValidCenter)) {
                    bestValidMonitor = i;
                    bestValidDist    = dist;
                    bestValidCenter  = center;
                }
            }

            if (dist < bestDist || (dist == bestDist && center < bestCenter)) {
                bestMonitor = i;
                bestDist    = dist;
                bestCenter  = center;
            }
        }

        if (validCount > 0)
            return bestValidMonitor;

        return bestMonitor;
    }

    std::optional<size_t> selectMonitorForWaylandPoint(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling) {
        return selectMonitorGeneric(monitors, point,
                                    [&](const PHLMONITOR& monitor) {
                                        const auto local = point - monitor->m_position;
                                        return containsPoint(local, {}, monitor->m_size);
                                    },
                                    [](const PHLMONITOR& monitor) { return monitor->m_position; }, [](const PHLMONITOR& monitor) { return monitor->m_size; });
    }

    std::optional<size_t> selectMonitorForXWaylandPoint(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling) {
        return selectMonitorGeneric(monitors, point,
                                    [&](const PHLMONITOR& monitor) {
                                        const auto local = point - monitor->m_xwaylandPosition;
                                        return containsPoint(local, {}, effectiveMonitorSize(monitor, forceZeroScaling));
                                    },
                                    [](const PHLMONITOR& monitor) { return monitor->m_xwaylandPosition; },
                                    [&](const PHLMONITOR& monitor) { return effectiveMonitorSize(monitor, forceZeroScaling); });
    }

    Vector2D waylandToXWaylandCoords(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling, std::optional<size_t> preferred) {
        const auto monitorIndex = preferred ? preferred : selectMonitorForWaylandPoint(monitors, point, forceZeroScaling);
        if (!monitorIndex)
            return {};

        const auto& monitor = monitors[*monitorIndex];
        auto        result  = point - monitor->m_position;

        if (forceZeroScaling)
            result *= monitor->m_scale;

        result += monitor->m_xwaylandPosition;
        return result;
    }

    Vector2D xwaylandToWaylandCoords(std::span<const PHLMONITOR> monitors, const Vector2D& point, bool forceZeroScaling, std::optional<size_t> preferred) {
        const auto monitorIndex = preferred ? preferred : selectMonitorForXWaylandPoint(monitors, point, forceZeroScaling);
        if (!monitorIndex)
            return {};

        const auto& monitor = monitors[*monitorIndex];
        auto        result  = point - monitor->m_xwaylandPosition;

        if (forceZeroScaling)
            result /= monitor->m_scale;

        result += monitor->m_position;
        return result;
    }

}
