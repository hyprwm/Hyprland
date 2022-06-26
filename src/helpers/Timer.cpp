#include "Timer.hpp"

void CTimer::reset() {
    m_tpLastReset = std::chrono::system_clock::now();
}

std::chrono::system_clock::duration CTimer::getDuration() {
    return std::chrono::system_clock::now() - m_tpLastReset;
}

int CTimer::getMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(getDuration()).count();
}

float CTimer::getSeconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(getDuration()).count() / 1000.f;
}