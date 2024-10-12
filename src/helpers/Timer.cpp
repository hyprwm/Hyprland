#include "Timer.hpp"
#include <chrono>

void CTimer::reset() {
    m_tpLastReset = std::chrono::steady_clock::now();
}

std::chrono::steady_clock::duration CTimer::getDuration() {
    return std::chrono::steady_clock::now() - m_tpLastReset;
}

float CTimer::getMillis() {
    return std::chrono::duration_cast<std::chrono::microseconds>(getDuration()).count() / 1000.f;
}

float CTimer::getSeconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(getDuration()).count() / 1000.f;
}

const std::chrono::steady_clock::time_point& CTimer::chrono() const {
    return m_tpLastReset;
}