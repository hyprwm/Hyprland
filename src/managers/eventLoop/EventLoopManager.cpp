#include "EventLoopManager.hpp"
#include "../../debug/Log.hpp"

#include <algorithm>
#include <limits>

#include <sys/timerfd.h>
#include <time.h>

#define TIMESPEC_NSEC_PER_SEC 1000000000L

CEventLoopManager::CEventLoopManager() {
    m_sTimers.timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
}

CEventLoopManager::~CEventLoopManager() {
    if (m_sWayland.eventSource)
        wl_event_source_remove(m_sWayland.eventSource);
}

static int timerWrite(int fd, uint32_t mask, void* data) {
    g_pEventLoopManager->onTimerFire();
    return 1;
}

void CEventLoopManager::enterLoop(wl_display* display, wl_event_loop* wlEventLoop) {
    m_sWayland.loop    = wlEventLoop;
    m_sWayland.display = display;

    m_sWayland.eventSource = wl_event_loop_add_fd(wlEventLoop, m_sTimers.timerfd, WL_EVENT_READABLE, timerWrite, nullptr);

    wl_display_run(display);

    Debug::log(LOG, "Kicked off the event loop! :(");
}

void CEventLoopManager::onTimerFire() {
    for (auto& t : m_sTimers.timers) {
        if (t.strongRef() > 1 /* if it's 1, it was lost. Don't call it. */ && t->passed() && !t->cancelled())
            t->call(t);
    }

    nudgeTimers();
}

void CEventLoopManager::addTimer(SP<CEventLoopTimer> timer) {
    m_sTimers.timers.push_back(timer);
    nudgeTimers();
}

void CEventLoopManager::removeTimer(SP<CEventLoopTimer> timer) {
    std::erase_if(m_sTimers.timers, [timer](const auto& t) { return timer == t; });
    nudgeTimers();
}

static void timespecAddNs(timespec* pTimespec, int64_t delta) {
    int delta_ns_low = delta % TIMESPEC_NSEC_PER_SEC;
    int delta_s_high = delta / TIMESPEC_NSEC_PER_SEC;

    pTimespec->tv_sec += delta_s_high;

    pTimespec->tv_nsec += (long)delta_ns_low;
    if (pTimespec->tv_nsec >= TIMESPEC_NSEC_PER_SEC) {
        pTimespec->tv_nsec -= TIMESPEC_NSEC_PER_SEC;
        ++pTimespec->tv_sec;
    }
}

void CEventLoopManager::nudgeTimers() {
    // remove timers that have gone missing
    std::erase_if(m_sTimers.timers, [](const auto& t) { return t.strongRef() <= 1; });

    long nextTimerUs = 10 * 1000 * 1000; // 10s

    for (auto& t : m_sTimers.timers) {
        if (const auto µs = t->leftUs(); µs < nextTimerUs)
            nextTimerUs = µs;
    }

    nextTimerUs = std::clamp(nextTimerUs + 1, 1L, std::numeric_limits<long>::max());

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    timespecAddNs(&now, nextTimerUs * 1000L);

    itimerspec ts = {.it_value = now};

    timerfd_settime(m_sTimers.timerfd, TFD_TIMER_ABSTIME, &ts, nullptr);
}