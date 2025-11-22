#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <wayland-server.h>
#include "../../helpers/signal/Signal.hpp"
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

    // schedules a recalc of the timers
    void scheduleRecalc();

    // schedules a function to run later, aka in a wayland idle event.
    void doLater(const std::function<void()>& fn);

    struct SIdleData {
        wl_event_source*                   eventSource = nullptr;
        std::vector<std::function<void()>> fns;
    };

    struct SReadableWaiter {
        wl_event_source*               source;
        Hyprutils::OS::CFileDescriptor fd;
        std::function<void()>          fn;

        SReadableWaiter(wl_event_source* src, Hyprutils::OS::CFileDescriptor f, std::function<void()> func) : source(src), fd(std::move(f)), fn(std::move(func)) {}

        ~SReadableWaiter() {
            if (source) {
                wl_event_source_remove(source);
                source = nullptr;
            }
        }

        // copy
        SReadableWaiter(const SReadableWaiter&)            = delete;
        SReadableWaiter& operator=(const SReadableWaiter&) = delete;

        // move
        SReadableWaiter(SReadableWaiter&& other) noexcept            = default;
        SReadableWaiter& operator=(SReadableWaiter&& other) noexcept = default;
    };

    // schedule function to when fd is readable (WL_EVENT_READABLE / POLLIN),
    // takes ownership of fd
    void doOnReadable(Hyprutils::OS::CFileDescriptor fd, std::function<void()>&& fn);
    void onFdReadable(SReadableWaiter* waiter);

  private:
    // Manages the event sources after AQ pollFDs change.
    void syncPollFDs();
    void nudgeTimers();

    struct SEventSourceData {
        SP<Aquamarine::SPollFD> pollFD;
        wl_event_source*        eventSource = nullptr;
    };

    struct {
        wl_event_loop*   loop        = nullptr;
        wl_display*      display     = nullptr;
        wl_event_source* eventSource = nullptr;
    } m_wayland;

    struct {
        std::vector<SP<CEventLoopTimer>> timers;
        Hyprutils::OS::CFileDescriptor   timerfd;
        bool                             recalcScheduled = false;
    } m_timers;

    SIdleData                        m_idle;
    std::map<int, SEventSourceData>  m_aqEventSources;
    std::vector<UP<SReadableWaiter>> m_readableWaiters;

    struct {
        CHyprSignalListener pollFDsChanged;
    } m_listeners;

    wl_event_source* m_configWatcherInotifySource = nullptr;

    friend class CAsyncDialogBox;
    friend class CMainLoopExecutor;
};

inline UP<CEventLoopManager> g_pEventLoopManager;
