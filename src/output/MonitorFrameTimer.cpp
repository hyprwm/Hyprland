#include "MonitorFrameTimer.hpp"
#include <algorithm>
#include <ranges>

using namespace Monitor;

// everything after we stop measuring - the commit, the atomic ioctl, the flip itself - plus timer wakeup jitter.
constexpr Time::steady_dur FRAME_SAFETY_MARGIN = std::chrono::microseconds(800);
// with no pageflip behind the frame event we have no vblank to aim at, so there is no margin use. render now.
constexpr Time::steady_dur FRAME_IDLE_DELAY = std::chrono::microseconds(1);

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

Time::steady_dur CMonitorFrameTimer::estimatedRenderCost() const {
    if (m_renderTimes.empty())
        return Time::steady_dur::zero();

    const auto WORST = std::ranges::max(m_renderTimes | std::views::transform([](const SRenderTimes& t) { return t.end - t.start; }));

    // 15 previous samples, take the slowest and assume the next can be that slow + 25% + the safety margin.
    // spikes pull the estimate up and stay for the whole window. until frames are more
    // stable and we have proper early outs, we cant be more precise.
    return WORST + WORST / 4 + FRAME_SAFETY_MARGIN;
}

void CMonitorFrameTimer::setRefreshPeriod(int refreshNs, float fallbackHz) {
    // drm hands us the period in ns, but its 0 if the connector has no refresh.
    if (refreshNs > 0) {
        m_refreshPeriod = std::chrono::nanoseconds(refreshNs);
        return;
    }

    const float HZ  = fallbackHz > 0.F ? fallbackHz : 60.F;
    m_refreshPeriod = std::chrono::nanoseconds(static_cast<int64_t>(1'000'000'000.0 / HZ));
}

Time::steady_dur CMonitorFrameTimer::refreshPeriod() const {
    return m_refreshPeriod;
}

bool CMonitorFrameTimer::hasRefreshPeriod() const {
    return m_refreshPeriod > Time::steady_dur::zero();
}

std::optional<Time::steady_dur> CMonitorFrameTimer::flipMiss(const Time::steady_tp& when, const Time::steady_tp& aimedAt) const {
    if (aimedAt.time_since_epoch() <= Time::steady_dur::zero() || !hasRefreshPeriod())
        return std::nullopt;

    // under half a period we made the flip, over a few periods the two aren't the same frame anymore.
    const auto LATE = when - aimedAt;
    if (LATE <= m_refreshPeriod / 2 || LATE >= m_refreshPeriod * 4)
        return std::nullopt;

    return LATE;
}

CMonitorFrameTimer::SFrameTarget CMonitorFrameTimer::nextTarget(const Time::steady_tp& now, const Time::steady_tp& earliestFlip, bool noRenderCost) const {
    if (earliestFlip.time_since_epoch() <= Time::steady_dur::zero() || !hasRefreshPeriod())
        return {.deadline = {}, .target = FRAME_IDLE_DELAY};

    const auto DEADLINE = std::min(earliestFlip, now + m_refreshPeriod);
    // nothing to render still costs us the commit, the ioctl and the timer wakeup - that is what the margin is for.
    const auto COST   = noRenderCost ? FRAME_SAFETY_MARGIN : estimatedRenderCost();
    const auto TARGET = DEADLINE - COST;

    // only delay if the estimate says we can still make this flip. with no render there is nothing to estimate.
    const bool CAN_DELAY = (noRenderCost || hasSamples()) && COST < m_refreshPeriod && TARGET > now;

    return {.deadline = DEADLINE, .target = CAN_DELAY ? TARGET - now : FRAME_IDLE_DELAY};
}
