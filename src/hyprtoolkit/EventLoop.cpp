#include "EventLoop.hpp"

#include <hyprtoolkit/core/Timer.hpp>
#include <hyprutils/memory/Casts.hpp>
#include <wayland-server-core.h>

#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <tuple>

#include "../debug/log/Logger.hpp"

using namespace Toolkit;

// wayland timers are in ms, and 0 disarms them
static int timerMs(float ms) {
    return std::max(1, sc<int>(std::ceil(ms)));
}

CEventLoop::SFdSource::~SFdSource() {
    if (source)
        wl_event_source_remove(source);
}

CEventLoop::STimerSource::~STimerSource() {
    if (source)
        wl_event_source_remove(source);
}

CEventLoop::CEventLoop(wl_event_loop* loop) : m_loop(loop), m_idleFd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {
    if (!m_idleFd.isValid()) {
        LOG(Log::ERR, "[hyprtoolkit] failed to create the idle eventfd");
        return;
    }

    m_idleSource = wl_event_loop_add_fd(m_loop, m_idleFd.get(), WL_EVENT_READABLE, onIdleWake, this);
}

CEventLoop::~CEventLoop() {
    m_fds.clear();
    m_timers.clear();

    if (m_idleSource)
        wl_event_source_remove(m_idleSource);
}

void CEventLoop::addFd(int fd, std::function<void()>&& callback) {
    removeFd(fd);

    auto source    = makeUnique<SFdSource>();
    source->fn     = std::move(callback);
    source->source = wl_event_loop_add_fd(m_loop, fd, WL_EVENT_READABLE, onFd, source.get());

    if (!source->source) {
        LOG(Log::ERR, "[hyprtoolkit] failed to add fd {} to the event loop", fd);
        return;
    }

    m_fds.emplace(fd, std::move(source));
}

void CEventLoop::removeFd(int fd) {
    m_fds.erase(fd);
}

int CEventLoop::onFd(int fd, uint32_t mask, void* data) {
    // copy, the callback may remove its own source
    const auto FN = sc<SFdSource*>(data)->fn;
    FN();

    return 0;
}

ASP<Hyprtoolkit::CTimer> CEventLoop::addTimer(const Hyprtoolkit::TimerDuration& timeout, std::function<void(ASP<Hyprtoolkit::CTimer> self, void* data)> callback, void* data,
                                              bool force) {
    auto timer = makeAtomicShared<Hyprtoolkit::CTimer>(timeout, std::move(callback), data, force);

    auto source    = makeUnique<STimerSource>();
    source->self   = this;
    source->timer  = timer;
    source->source = wl_event_loop_add_timer(m_loop, onTimer, source.get());

    if (!source->source) {
        LOG(Log::ERR, "[hyprtoolkit] failed to add a timer to the event loop");
        return timer;
    }

    wl_event_source_timer_update(source->source, timerMs(std::chrono::duration<float, std::milli>(timeout).count()));
    m_timers.emplace_back(std::move(source));

    return timer;
}

int CEventLoop::onTimer(void* data) {
    auto*      source = sc<STimerSource*>(data);
    auto*      self   = source->self;
    const auto TIMER  = source->timer;

    // the timer may have been pushed back after it was armed
    if (!TIMER->cancelled() && !TIMER->passed()) {
        wl_event_source_timer_update(source->source, timerMs(TIMER->leftMs()));
        return 0;
    }

    // one-shot, drop the source before calling into the toolkit, which may add timers
    std::erase_if(self->m_timers, [source](const auto& t) { return t.get() == source; });

    if (!TIMER->cancelled())
        TIMER->call(TIMER);

    return 0;
}

void CEventLoop::addIdle(const std::function<void()>& callback) {
    {
        std::lock_guard lock(m_idleMutex);
        m_idles.emplace_back(callback);
    }

    const uint64_t ONE = 1;
    std::ignore        = write(m_idleFd.get(), &ONE, sizeof(ONE));
}

int CEventLoop::onIdleWake(int fd, uint32_t mask, void* data) {
    uint64_t count = 0;
    std::ignore    = read(fd, &count, sizeof(count));

    sc<CEventLoop*>(data)->dispatchIdles();
    return 0;
}

void CEventLoop::dispatchIdles() {
    std::vector<std::function<void()>> idles;

    {
        std::lock_guard lock(m_idleMutex);
        idles.swap(m_idles);
    }

    for (const auto& idle : idles) {
        idle();
    }
}

void CEventLoop::cancelPending() {
    {
        std::lock_guard lock(m_idleMutex);
        m_idles.clear();
    }

    m_timers.clear();
}

void CEventLoop::enterLoop() {
    ; // the compositor's loop is already running
}
