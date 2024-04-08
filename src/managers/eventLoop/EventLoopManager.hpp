#pragma once

#include <condition_variable>
#include <mutex>
#include <memory>
#include <thread>
#include <wayland-server.h>

#include "EventLoopTimer.hpp"

class CEventLoopManager {
  public:
    CEventLoopManager();

    void enterLoop(wl_display* display, wl_event_loop* wlEventLoop);
    void addTimer(std::shared_ptr<CEventLoopTimer> timer);
    void removeTimer(std::shared_ptr<CEventLoopTimer> timer);

    void onTimerFire();

    // recalculates timers
    void nudgeTimers();

  private:
    struct {
        wl_event_loop* loop    = nullptr;
        wl_display*    display = nullptr;
    } m_sWayland;

    struct {
        std::vector<std::shared_ptr<CEventLoopTimer>> timers;
        int                                           timerfd = -1;
    } m_sTimers;
};

inline std::unique_ptr<CEventLoopManager> g_pEventLoopManager;