#include "Timer.hpp"

#define chr std::chrono

void CTimer::reset() {
    m_lastReset = Time::steadyNow();
}

Time::steady_dur CTimer::getDuration() {
    return Time::steadyNow() - m_lastReset;
}

float CTimer::getMillis() {
    return chr::duration_cast<chr::microseconds>(getDuration()).count() / 1000.F;
}

float CTimer::getSeconds() {
    return chr::duration_cast<chr::milliseconds>(getDuration()).count() / 1000.F;
}

const Time::steady_tp& CTimer::chrono() const {
    return m_lastReset;
}