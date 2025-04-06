#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <wayland-server.h>
#include "helpers/signal/Signal.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

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
    // Manages the event sources after AQ pollFDs change.
    void syncPollFDs();

    struct SEventSourceData {
        SP<Aquamarine::SPollFD> pollFD;
        wl_event_source*        eventSource = nullptr;
    };

    struct {
        wl_event_loop*   loop        = nullptr;
        wl_display*      display     = nullptr;
        wl_event_source* eventSource = nullptr;
    } m_sWayland;

    struct {
        std::vector<SP<CEventLoopTimer>> timers;
        Hyprutils::OS::CFileDescriptor   timerfd;
    } m_sTimers;

    SIdleData                       m_sIdle;
    std::map<int, SEventSourceData> aqEventSources;

    struct {
        CHyprSignalListener pollFDsChanged;
    } m_sListeners;

    wl_event_source* m_configWatcherInotifySource = nullptr;

    friend class CSyncTimeline;
    friend class CAsyncDialogBox;
};

inline UP<CEventLoopManager> g_pEventLoopManager;
