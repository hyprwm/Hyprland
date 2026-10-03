#include "BlurAnimationClock.hpp"

#include <algorithm>

using namespace Render::GL;

CBlurAnimationClock::CBlurAnimationClock(Time::steady_tp now) : m_lastUpdate(now) {
    ;
}

double CBlurAnimationClock::sample(Time::steady_tp now) const {
    // A config hook can advance the live anchor past the capture's fixed timestamp.
    const auto ELAPSED = std::max(std::chrono::duration<double>(now - m_lastUpdate).count(), 0.0);
    return m_time + ELAPSED * m_previousSpeed;
}

double CBlurAnimationClock::update(Time::steady_tp now, float speed) {
    m_time += std::chrono::duration<double>(now - m_lastUpdate).count() * m_previousSpeed;
    m_lastUpdate    = now;
    m_previousSpeed = speed;
    return m_time;
}
