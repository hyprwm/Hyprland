#include "EventLoopTimer.hpp"
#include <limits>
#include "EventLoopManager.hpp"
#include "../../helpers/time/Time.hpp"

CEventLoopTimer::CEventLoopTimer(std::optional<Time::steady_dur> timeout, std::function<void(SP<CEventLoopTimer> self, void* data)> cb_, void* data_) : m_cb(cb_), m_data(data_) {

    if (!timeout.has_value())
        m_expires.reset();
    else
        m_expires = Time::steadyNow() + *timeout;
}

void CEventLoopTimer::updateTimeout(std::optional<Time::steady_dur> timeout) {
    if (!timeout.has_value()) {
        m_expires.reset();
        g_pEventLoopManager->nudgeTimers();
        return;
    }

    m_expires = Time::steadyNow() + *timeout;

    g_pEventLoopManager->nudgeTimers();
}

bool CEventLoopTimer::passed() {
    if (!m_expires.has_value())
        return false;
    return Time::steadyNow() > *m_expires;
}

void CEventLoopTimer::cancel() {
    m_wasCancelled = true;
    m_expires.reset();
}

bool CEventLoopTimer::cancelled() {
    return m_wasCancelled;
}

void CEventLoopTimer::call(SP<CEventLoopTimer> self) {
    m_expires.reset();
    m_cb(self, m_data);
}

float CEventLoopTimer::leftUs() {
    if (!m_expires.has_value())
        return std::numeric_limits<float>::max();

    return std::chrono::duration_cast<std::chrono::microseconds>(*m_expires - Time::steadyNow()).count();
}

bool CEventLoopTimer::armed() {
    return m_expires.has_value();
}
