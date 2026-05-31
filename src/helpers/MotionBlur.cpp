#include "MotionBlur.hpp"

#include <algorithm>
#include <chrono>

using namespace MotionBlur;

CBox MotionBlur::extents(const CBox& previous, const CBox& current) {
    const double x1 = std::min(previous.x, current.x);
    const double y1 = std::min(previous.y, current.y);
    const double x2 = std::max(previous.x + previous.w, current.x + current.w);
    const double y2 = std::max(previous.y + previous.h, current.y + current.h);

    return {x1, y1, x2 - x1, y2 - y1};
}

CBox SState::extents() const {
    return MotionBlur::extents(previous, current);
}

void CTracker::record(const CBox& previous, const CBox& current) {
    m_previous  = previous;
    m_current   = current;
    m_updatedAt = Time::steadyNow();
    m_valid     = true;
}

void CTracker::reset() {
    m_previous = {};
    m_current  = {};
    m_valid    = false;
}

std::optional<SState> CTracker::state(int samples, const Vector2D& offset, bool allowStale) const {
    if (!m_valid)
        return std::nullopt;

    const float AGE = std::chrono::duration<float, std::milli>(Time::steadyNow() - m_updatedAt).count();
    if (!allowStale && AGE > 100.F)
        return std::nullopt;

    CBox previous = m_previous.copy().translate(offset);
    CBox current  = m_current.copy().translate(offset);

    if (previous == current || previous.w <= 0.F || previous.h <= 0.F || current.w <= 0.F || current.h <= 0.F)
        return std::nullopt;

    return SState{
        .previous = previous,
        .current  = current,
        .samples  = samples,
    };
}
