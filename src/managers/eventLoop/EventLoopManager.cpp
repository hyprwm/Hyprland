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
    m_timers.timerfd  = CFileDescriptor{timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)};
    m_wayland.loop    = wlEventLoop;
    m_wayland.display = display;
}

CEventLoopManager::~CEventLoopManager() {
    for (auto const& [_, eventSourceData] : m_aqEventSources) {
        wl_event_source_remove(eventSourceData.eventSource);
    }

    for (auto const& w : m_readableWaiters) {
        if (w->source != nullptr)
            wl_event_source_remove(w->source);
    }

    if (m_wayland.eventSource)
        wl_event_source_remove(m_wayland.eventSource);
    if (m_idle.eventSource)
        wl_event_source_remove(m_idle.eventSource);
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

static int handleWaiterFD(int fd, uint32_t mask, void* data) {
    if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
        Debug::log(ERR, "handleWaiterFD: readable waiter error");
        return 0;
    }

    if (mask & WL_EVENT_READABLE)
        g_pEventLoopManager->onFdReadable((CEventLoopManager::SReadableWaiter*)data);

    return 0;
}

void CEventLoopManager::onFdReadable(SReadableWaiter* waiter) {
    auto it = std::ranges::find_if(m_readableWaiters, [waiter](const UP<SReadableWaiter>& w) { return waiter == w.get() && w->fd == waiter->fd && w->source == waiter->source; });

    // ???
    if (it == m_readableWaiters.end())
        return;

    UP<SReadableWaiter> taken = std::move(*it);
    m_readableWaiters.erase(it);

    if (taken->fn)
        taken->fn();
}

void CEventLoopManager::enterLoop() {
    m_wayland.eventSource = wl_event_loop_add_fd(m_wayland.loop, m_timers.timerfd.get(), WL_EVENT_READABLE, timerWrite, nullptr);

    if (const auto& FD = g_pConfigWatcher->getInotifyFD(); FD.isValid())
        m_configWatcherInotifySource = wl_event_loop_add_fd(m_wayland.loop, FD.get(), WL_EVENT_READABLE, configWatcherWrite, nullptr);

    syncPollFDs();
    m_listeners.pollFDsChanged = g_pCompositor->m_aqBackend->events.pollFDsChanged.listen([this] { syncPollFDs(); });

    // if we have a session, dispatch it to get the pending input devices
    if (g_pCompositor->m_aqBackend->hasSession())
        g_pCompositor->m_aqBackend->session->dispatchPendingEventsAsync();

    wl_display_run(m_wayland.display);

    Debug::log(LOG, "Kicked off the event loop! :(");
}

void CEventLoopManager::onTimerFire() {
    const auto CPY = m_timers.timers;
    for (auto const& t : CPY) {
        if (t.strongRef() > 2 /* if it's 2, it was lost. Don't call it. */ && t->passed() && !t->cancelled())
            t->call(t);
    }

    scheduleRecalc();
}

void CEventLoopManager::addTimer(SP<CEventLoopTimer> timer) {
    if (std::ranges::contains(m_timers.timers, timer))
        return;
    m_timers.timers.emplace_back(timer);
    scheduleRecalc();
}

void CEventLoopManager::removeTimer(SP<CEventLoopTimer> timer) {
    if (!std::ranges::contains(m_timers.timers, timer))
        return;
    std::erase_if(m_timers.timers, [timer](const auto& t) { return timer == t; });
    scheduleRecalc();
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

void CEventLoopManager::scheduleRecalc() {
    // do not do it instantly, do it later. Avoid recursive access to the timer
    // vector, it could be catastrophic if we modify it while iterating

    if (m_timers.recalcScheduled)
        return;

    m_timers.recalcScheduled = true;

    doLater([this] { nudgeTimers(); });
}

void CEventLoopManager::nudgeTimers() {
    m_timers.recalcScheduled = false;

    // remove timers that have gone missing
    std::erase_if(m_timers.timers, [](const auto& t) { return t.strongRef() <= 1; });

    long nextTimerUs = 10L * 1000 * 1000; // 10s

    for (auto const& t : m_timers.timers) {
        if (auto const& µs = t->leftUs(); µs < nextTimerUs)
            nextTimerUs = µs;
    }

    nextTimerUs = std::clamp(nextTimerUs + 1, 1L, std::numeric_limits<long>::max());

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    timespecAddNs(&now, nextTimerUs * 1000L);

    itimerspec ts = {.it_value = now};

    timerfd_settime(m_timers.timerfd.get(), TFD_TIMER_ABSTIME, &ts, nullptr);
}

void CEventLoopManager::doLater(const std::function<void()>& fn) {
    m_idle.fns.emplace_back(fn);

    if (m_idle.eventSource)
        return;

    m_idle.eventSource = wl_event_loop_add_idle(
        m_wayland.loop,
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
        &m_idle);
}

void CEventLoopManager::doOnReadable(CFileDescriptor fd, const std::function<void()>& fn) {
    if (!fd.isValid() || fd.isReadable()) {
        fn();
        return;
    }

    auto& waiter   = m_readableWaiters.emplace_back(makeUnique<SReadableWaiter>(nullptr, std::move(fd), fn));
    waiter->source = wl_event_loop_add_fd(g_pEventLoopManager->m_wayland.loop, waiter->fd.get(), WL_EVENT_READABLE, ::handleWaiterFD, waiter.get());
}

void CEventLoopManager::syncPollFDs() {
    auto aqPollFDs = g_pCompositor->m_aqBackend->getPollFDs();

    std::erase_if(m_aqEventSources, [&](const auto& item) {
        auto const& [fd, eventSourceData] = item;

        // If no pollFD has the same fd, remove this event source
        const bool shouldRemove = std::ranges::none_of(aqPollFDs, [&](const auto& pollFD) { return pollFD->fd == fd; });

        if (shouldRemove)
            wl_event_source_remove(eventSourceData.eventSource);

        return shouldRemove;
    });

    for (auto& fd : aqPollFDs | std::views::filter([&](SP<Aquamarine::SPollFD> fd) { return !m_aqEventSources.contains(fd->fd); })) {
        auto eventSource         = wl_event_loop_add_fd(m_wayland.loop, fd->fd, WL_EVENT_READABLE, aquamarineFDWrite, fd.get());
        m_aqEventSources[fd->fd] = {.pollFD = fd, .eventSource = eventSource};
    }
}
