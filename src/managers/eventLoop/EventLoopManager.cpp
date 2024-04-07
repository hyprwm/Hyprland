#include "EventLoopManager.hpp"
#include "../../debug/Log.hpp"

#include <algorithm>

#include <sys/poll.h>

void CEventLoopManager::enterLoop(wl_display* display, wl_event_loop* wlEventLoop) {
    m_sWayland.loop    = wlEventLoop;
    m_sWayland.display = display;

    pollfd pollfds[] = {
        {
            .fd     = wl_event_loop_get_fd(wlEventLoop),
            .events = POLLIN,
        },
    };

    std::thread pollThr([this, &pollfds]() {
        while (!m_bTerminate) {
            int ret = poll(pollfds, 1, 5000 /* 5 seconds, reasonable. Just in case we need to terminate and the signal fails */);

            if (ret < 0) {
                if (errno == EINTR)
                    continue;

                Debug::log(CRIT, "Polling fds failed with {}", errno);
                m_bTerminate = true;
                return;
            }

            for (size_t i = 0; i < 1; ++i) {
                if (pollfds[i].revents & POLLHUP) {
                    Debug::log(CRIT, "Disconnected from pollfd id {}", i);
                    m_bTerminate = true;
                    return;
                }
            }

            if (ret != 0) {
                {
                    std::lock_guard<std::mutex> lg2(m_sLoopState.eventRequestMutex);
                    std::lock_guard<std::mutex> lg(m_sLoopState.loopMutex);
                    m_sLoopState.event = true;
                }
                m_sLoopState.cv.notify_all();
            }
        }
    });

    std::thread timersThr([this]() {
        while (!m_bTerminate) {
            // calc nearest thing

            m_sTimers.timersMutex.lock();
            float least = 1000 * 1000 * 10; // 10s in µs
            for (auto& t : m_sTimers.timers) {
                const auto TIME = std::clamp(t->leftUs(), 0.f, INFINITY);
                if (TIME < least)
                    least = TIME;
            }
            m_sTimers.timersMutex.unlock();

            if (least > 0) {
                std::unique_lock lk(m_sTimers.timersRqMutex);
                m_sTimers.cv.wait_for(lk, std::chrono::microseconds((int)least + 1), [this] { return m_sTimers.event; });
                m_sTimers.event = false;
            }

            // notify main
            {
                std::lock_guard<std::mutex> lg2(m_sLoopState.eventRequestMutex);
                std::lock_guard<std::mutex> lg(m_sLoopState.loopMutex);
                m_sLoopState.event = true;
            }

            m_sLoopState.cv.notify_all();
        }
    });

    m_sLoopState.event = true; // let it process once

    while (1) {
        std::unique_lock lk(m_sLoopState.eventRequestMutex);
        m_sLoopState.cv.wait_for(lk, std::chrono::milliseconds(5000), [this] { return m_sLoopState.event; });

        if (m_bTerminate)
            break;

        std::lock_guard<std::mutex> lg(m_sLoopState.loopMutex);

        m_sLoopState.event = false;

        if (pollfds[0].revents & POLLIN /* wl event loop */) {
            wl_display_flush_clients(m_sWayland.display);
            if (wl_event_loop_dispatch(m_sWayland.loop, -1) < 0) {
                m_bTerminate = true;
                break;
            }
        }

        // TODO: don't check timers without the timer thread requesting it
        // I tried but it didnt work :/

        m_sTimers.timersMutex.lock();
        auto timerscpy = m_sTimers.timers;
        m_sTimers.timersMutex.unlock();

        for (auto& t : timerscpy) {
            if (t->passed() && !t->cancelled())
                t->call(t);
        }

        if (m_bTerminate)
            break;
    }

    Debug::log(LOG, "Kicked off the event loop! :(");

    m_sTimers.event = true;
    m_sTimers.cv.notify_all();
}

void CEventLoopManager::addTimer(std::shared_ptr<CEventLoopTimer> timer) {
    m_sTimers.timersMutex.lock();
    m_sTimers.timers.push_back(timer);
    m_sTimers.timersMutex.unlock();
    nudgeTimers();
}

void CEventLoopManager::removeTimer(std::shared_ptr<CEventLoopTimer> timer) {
    m_sTimers.timersMutex.lock();
    std::erase_if(m_sTimers.timers, [timer](const auto& t) { return timer == t; });
    m_sTimers.timersMutex.unlock();
}

void CEventLoopManager::nudgeTimers() {
    m_sTimers.event = true;
    m_sTimers.cv.notify_all();
}