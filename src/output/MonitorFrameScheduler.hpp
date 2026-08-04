#pragma once

#include "Monitor.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"

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
        Time::steady_tp            m_earliestNextFlip;
        Time::steady_dur           m_refreshPeriod{};
        bool                       m_delayNextFrame = false;

        friend class CMonitor;
    };
}
