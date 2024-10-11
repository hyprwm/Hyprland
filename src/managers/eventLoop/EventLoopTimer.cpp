#include "EventLoopTimer.hpp"
#include <limits>
#include "EventLoopManager.hpp"

CEventLoopTimer::CEventLoopTimer(std::optional<std::chrono::steady_clock::duration> timeout, std::function<void(SP<CEventLoopTimer> self, void* data)> cb_, void* data_) :
    cb(cb_), data(data_) {

    if (!timeout.has_value())
        expires.reset();
    else
        expires = std::chrono::steady_clock::now() + *timeout;
}

void CEventLoopTimer::updateTimeout(std::optional<std::chrono::steady_clock::duration> timeout) {
    if (!timeout.has_value()) {
        expires.reset();
        g_pEventLoopManager->nudgeTimers();
        return;
    }

    expires = std::chrono::steady_clock::now() + *timeout;

    g_pEventLoopManager->nudgeTimers();
}

bool CEventLoopTimer::passed() {
    if (!expires.has_value())
        return false;
    return std::chrono::steady_clock::now() > *expires;
}

void CEventLoopTimer::cancel() {
    wasCancelled = true;
    expires.reset();
}

bool CEventLoopTimer::cancelled() {
    return wasCancelled;
}

void CEventLoopTimer::call(SP<CEventLoopTimer> self) {
    expires.reset();
    cb(self, data);
}

float CEventLoopTimer::leftUs() {
    if (!expires.has_value())
        return std::numeric_limits<float>::max();

    return std::chrono::duration_cast<std::chrono::microseconds>(*expires - std::chrono::steady_clock::now()).count();
}

bool CEventLoopTimer::armed() {
    return expires.has_value();
}
