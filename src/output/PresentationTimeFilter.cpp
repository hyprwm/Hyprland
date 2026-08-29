#include "PresentationTimeFilter.hpp"

using namespace Monitor;

Time::steady_tp CPresentationTimeFilter::filter(const Time::steady_tp& raw, Time::steady_dur period) {
    // we have no calculated vblank periods, just use the raw wrong timestamps.
    if (period <= Time::steady_dur::zero()) {
        m_previousTimeSample.reset();
        return raw;
    }

    // no previous samples yet
    if (!m_previousTimeSample) {
        m_previousTimeSample = raw;
        return raw;
    }

    const auto EXPECTED = *m_previousTimeSample + period;
    const auto JITTER   = raw - EXPECTED;

    // missed vblank / idleframe. reset.
    if (JITTER > period / 2 || JITTER < -period / 2) {
        m_previousTimeSample = raw;
        return raw;
    }

    m_previousTimeSample = EXPECTED;

    return EXPECTED;
}