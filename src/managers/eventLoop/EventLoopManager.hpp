#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <wayland-server.h>

#include "EventLoopTimer.hpp"

namespace Aquamarine {
    struct SPollFD;
};

class CEventLoopManager {
  public:
    CEventLoopManager(wl_display* display, wl_event_loop* wlEventLoop);
    ~CEventLoopManager();

    void enterLoop();

    // Note: will remove the timer if the ptr is lost.
    void addTimer(SP<CEventLoopTimer> timer);
    void removeTimer(SP<CEventLoopTimer> timer);

    void onTimerFire();

    // recalculates timers
    void nudgeTimers();

    // schedules a function to run later, aka in a wayland idle event.
    void doLater(const std::function<void()>& fn);

    struct SIdleData {
        wl_event_source*                   eventSource = nullptr;
        std::vector<std::function<void()>> fns;
    };

  private:
    struct {
        wl_event_loop*                loop        = nullptr;
        wl_display*                   display     = nullptr;
        wl_event_source*              eventSource = nullptr;
        std::vector<wl_event_source*> aqEventSources;
    } m_sWayland;

    struct {
        std::vector<SP<CEventLoopTimer>> timers;
        CFileDescriptor                  timerfd;
    } m_sTimers;

    SIdleData                            m_sIdle;
    std::vector<SP<Aquamarine::SPollFD>> aqPollFDs;

    friend class CSyncTimeline;
};

inline std::unique_ptr<CEventLoopManager> g_pEventLoopManager;
