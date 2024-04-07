#include "EventLoopTimer.hpp"
#include <limits>
#include "EventLoopManager.hpp"

CEventLoopTimer::CEventLoopTimer(std::optional<std::chrono::system_clock::duration> timeout, std::function<void(std::shared_ptr<CEventLoopTimer> self, void* data)> cb_,
                                 void* data_) :
    cb(cb_),
    data(data_) {

    if (!timeout.has_value())
        expires.reset();
    else
        expires = std::chrono::system_clock::now() + *timeout;
}

void CEventLoopTimer::updateTimeout(std::optional<std::chrono::system_clock::duration> timeout) {
    if (!timeout.has_value()) {
        expires.reset();
        g_pEventLoopManager->nudgeTimers();
        return;
    }

    expires = std::chrono::system_clock::now() + *timeout;

    g_pEventLoopManager->nudgeTimers();
}

bool CEventLoopTimer::passed() {
    if (!expires.has_value())
        return false;
    return std::chrono::system_clock::now() > *expires;
}

void CEventLoopTimer::cancel() {
    wasCancelled = true;
    expires.reset();
}

bool CEventLoopTimer::cancelled() {
    return wasCancelled;
}

void CEventLoopTimer::call(std::shared_ptr<CEventLoopTimer> self) {
    expires.reset();
    cb(self, data);
}

float CEventLoopTimer::leftUs() {
    if (!expires.has_value())
        return std::numeric_limits<float>::max();

    return std::chrono::duration_cast<std::chrono::microseconds>(*expires - std::chrono::system_clock::now()).count();
}
