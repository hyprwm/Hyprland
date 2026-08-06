#pragma once

#include "Monitor.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"
#include "MonitorFrameTimer.hpp"

namespace Monitor {
    class CMonitorFrameScheduler {
      public:
        CMonitorFrameScheduler(PHLMONITOR m);
        ~CMonitorFrameScheduler();

        CMonitorFrameScheduler(const CMonitorFrameScheduler&)            = delete;
        CMonitorFrameScheduler(CMonitorFrameScheduler&&)                 = delete;
        CMonitorFrameScheduler& operator=(const CMonitorFrameScheduler&) = delete;
        CMonitorFrameScheduler& operator=(CMonitorFrameScheduler&&)      = delete;

        void                    onPresented(const Time::steady_tp& when, int refreshNs);
        void                    onFrame();
        bool                    renderPending();

      private:
        void                       renderNow();
        bool                       canRender();
        bool                       newSchedulingEnabled();

        PHLMONITORREF              m_monitor;
        WP<CMonitorFrameScheduler> m_self;
        SP<CEventLoopTimer>        m_renderTimer;
        CMonitorFrameTimer         m_frameTimes;
        Time::steady_tp            m_earliestNextFlip;
        Time::steady_tp            m_pendingDeadline;  // deadline of the frame the render timer is currently armed for
        Time::steady_tp            m_inFlightDeadline; // deadline of the frame that committed and is waiting on its flip
        bool                       m_delayNextFrame = false;

        friend class CMonitor;
    };
}
