#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <wayland-server.h>

#include "EventLoopTimer.hpp"

class CEventLoopManager {
  public:
    CEventLoopManager();
    ~CEventLoopManager();

    void enterLoop(wl_display* display, wl_event_loop* wlEventLoop);

    // Note: will remove the timer if the ptr is lost.
    void addTimer(SP<CEventLoopTimer> timer);
    void removeTimer(SP<CEventLoopTimer> timer);

    void onTimerFire();

    // recalculates timers
    void nudgeTimers();

  private:
    struct {
        wl_event_loop*   loop        = nullptr;
        wl_display*      display     = nullptr;
        wl_event_source* eventSource = nullptr;
    } m_sWayland;

    struct {
        std::vector<SP<CEventLoopTimer>> timers;
        int                              timerfd = -1;
    } m_sTimers;
};

inline std::unique_ptr<CEventLoopManager> g_pEventLoopManager;