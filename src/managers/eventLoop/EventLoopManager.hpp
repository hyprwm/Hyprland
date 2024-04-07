#pragma once

#include <condition_variable>
#include <mutex>
#include <memory>
#include <thread>
#include <wayland-server.h>

#include "EventLoopTimer.hpp"

class CEventLoopManager {
  public:
    void enterLoop(wl_display* display, wl_event_loop* wlEventLoop);
    void addTimer(std::shared_ptr<CEventLoopTimer> timer);
    void removeTimer(std::shared_ptr<CEventLoopTimer> timer);

    // recalculates timers
    void nudgeTimers();

  private:
    struct {
        wl_event_loop* loop    = nullptr;
        wl_display*    display = nullptr;
        std::thread    pollThread;
    } m_sWayland;

    struct {
        std::mutex                                    timersMutex;
        std::mutex                                    timersRqMutex;
        std::vector<std::shared_ptr<CEventLoopTimer>> timers;
        std::thread                                   timerThread;
        bool                                          event = false;
        std::condition_variable                       cv;
    } m_sTimers;

    struct {
        std::mutex              loopMutex;
        std::mutex              eventRequestMutex;
        bool                    event = false;
        std::condition_variable cv;
    } m_sLoopState;

    bool m_bTerminate = false;
};

inline std::unique_ptr<CEventLoopManager> g_pEventLoopManager;