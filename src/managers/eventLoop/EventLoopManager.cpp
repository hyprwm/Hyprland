#include "EventLoopManager.hpp"
#include "../../debug/Log.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigWatcher.hpp"

#include <algorithm>
#include <limits>
#include <ranges>

#include <sys/timerfd.h>
#include <ctime>

#include <aquamarine/backend/Backend.hpp>
using namespace Hyprutils::OS;

#define TIMESPEC_NSEC_PER_SEC 1000000000L

CEventLoopManager::CEventLoopManager(wl_display* display, wl_event_loop* wlEventLoop) {
    m_sTimers.timerfd  = CFileDescriptor{timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)};
    m_sWayland.loop    = wlEventLoop;
    m_sWayland.display = display;
}

CEventLoopManager::~CEventLoopManager() {
    for (auto const& [_, eventSourceData] : aqEventSources) {
        wl_event_source_remove(eventSourceData.eventSource);
    }

    if (m_sWayland.eventSource)
        wl_event_source_remove(m_sWayland.eventSource);
    if (m_sIdle.eventSource)
        wl_event_source_remove(m_sIdle.eventSource);
    if (m_configWatcherInotifySource)
        wl_event_source_remove(m_configWatcherInotifySource);
}

static int timerWrite(int fd, uint32_t mask, void* data) {
    g_pEventLoopManager->onTimerFire();
    return 1;
}

static int aquamarineFDWrite(int fd, uint32_t mask, void* data) {
    auto POLLFD = (Aquamarine::SPollFD*)data;
    POLLFD->onSignal();
    return 1;
}

static int configWatcherWrite(int fd, uint32_t mask, void* data) {
    g_pConfigWatcher->onInotifyEvent();
    return 0;
}

void CEventLoopManager::enterLoop() {
    m_sWayland.eventSource = wl_event_loop_add_fd(m_sWayland.loop, m_sTimers.timerfd.get(), WL_EVENT_READABLE, timerWrite, nullptr);

    if (const auto& FD = g_pConfigWatcher->getInotifyFD(); FD.isValid())
        m_configWatcherInotifySource = wl_event_loop_add_fd(m_sWayland.loop, FD.get(), WL_EVENT_READABLE, configWatcherWrite, nullptr);

    syncPollFDs();
    m_sm_listeners.pollFDsChanged = g_pCompositor->m_pAqBackend->events.pollFDsChanged.registerListener([this](std::any d) { syncPollFDs(); });

    // if we have a session, dispatch it to get the pending input devices
    if (g_pCompositor->m_pAqBackend->hasSession())
        g_pCompositor->m_pAqBackend->session->dispatchPendingEventsAsync();

    wl_display_run(m_sWayland.display);

    NDebug::log(LOG, "Kicked off the event loop! :(");
}

void CEventLoopManager::onTimerFire() {
    for (auto const& t : m_sTimers.timers) {
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
    auto delta_ns_low = delta % TIMESPEC_NSEC_PER_SEC;
    auto delta_s_high = delta / TIMESPEC_NSEC_PER_SEC;

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

    long nextTimerUs = 10L * 1000 * 1000; // 10s

    for (auto const& t : m_sTimers.timers) {
        if (auto const& µs = t->leftUs(); µs < nextTimerUs)
            nextTimerUs = µs;
    }

    nextTimerUs = std::clamp(nextTimerUs + 1, 1L, std::numeric_limits<long>::max());

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    timespecAddNs(&now, nextTimerUs * 1000L);

    itimerspec ts = {.it_value = now};

    timerfd_settime(m_sTimers.timerfd.get(), TFD_TIMER_ABSTIME, &ts, nullptr);
}

void CEventLoopManager::doLater(const std::function<void()>& fn) {
    m_sIdle.fns.emplace_back(fn);

    if (m_sIdle.eventSource)
        return;

    m_sIdle.eventSource = wl_event_loop_add_idle(
        m_sWayland.loop,
        [](void* data) {
            auto IDLE = (CEventLoopManager::SIdleData*)data;
            auto cpy  = IDLE->fns;
            IDLE->fns.clear();
            IDLE->eventSource = nullptr;
            for (auto const& c : cpy) {
                if (c)
                    c();
            }
        },
        &m_sIdle);
}

void CEventLoopManager::syncPollFDs() {
    auto aqPollFDs = g_pCompositor->m_pAqBackend->getPollFDs();

    std::erase_if(aqEventSources, [&](const auto& item) {
        auto const& [fd, eventSourceData] = item;

        // If no pollFD has the same fd, remove this event source
        const bool shouldRemove = std::ranges::none_of(aqPollFDs, [&](const auto& pollFD) { return pollFD->fd == fd; });

        if (shouldRemove)
            wl_event_source_remove(eventSourceData.eventSource);

        return shouldRemove;
    });

    for (auto& fd : aqPollFDs | std::views::filter([&](SP<Aquamarine::SPollFD> fd) { return !aqEventSources.contains(fd->fd); })) {
        auto eventSource       = wl_event_loop_add_fd(m_sWayland.loop, fd->fd, WL_EVENT_READABLE, aquamarineFDWrite, fd.get());
        aqEventSources[fd->fd] = {.pollFD = fd, .eventSource = eventSource};
    }
}
