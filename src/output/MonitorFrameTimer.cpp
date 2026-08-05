#include "MonitorFrameTimer.hpp"
#include <algorithm>
#include <ranges>

using namespace Monitor;

CMonitorFrameTimer::CMonitorFrameTimer() {
    ;
}

void CMonitorFrameTimer::addRenderCost(const Time::steady_tp& start, const Time::steady_tp& end) {
    // a fence that signalled before we started rendering is a stale one, left over from a frame that
    // didn't redraw. it measures nothing.
    if (end <= start)
        return;

    m_renderTimes.emplace_back(SRenderTimes{.start = start, .end = end});

    if (m_renderTimes.size() > 15)
        m_renderTimes.pop_front();
}

bool CMonitorFrameTimer::hasSamples() const {
    return !m_renderTimes.empty();
}

Time::steady_dur CMonitorFrameTimer::estimatedRenderCost() {
    if (m_renderTimes.empty())
        return Time::steady_dur::zero();

    const auto WORST = std::ranges::max(m_renderTimes | std::views::transform([](const SRenderTimes& t) { return t.end - t.start; }));

    return WORST + WORST / 4 + FRAME_SAFETY_MARGIN;
}
