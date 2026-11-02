#pragma once

#include "Monitor.hpp"
#include "../render/SyncFDManager.hpp"

#include <chrono>

namespace Monitor {
    class CMonitorFrameScheduler {
      public:
        using stc = std::chrono::steady_clock;

        CMonitorFrameScheduler(PHLMONITOR m);

        CMonitorFrameScheduler(const CMonitorFrameScheduler&)            = delete;
        CMonitorFrameScheduler(CMonitorFrameScheduler&&)                 = delete;
        CMonitorFrameScheduler& operator=(const CMonitorFrameScheduler&) = delete;
        CMonitorFrameScheduler& operator=(CMonitorFrameScheduler&&)      = delete;

        void                    onSyncFired();
        void                    onPresented();
        void                    onFrame();

      private:
        bool                       canRender();
        void                       onFinishRender();
        bool                       newSchedulingEnabled();

        bool                       m_renderAtFrame = true;
        bool                       m_pendingThird  = false;
        stc::time_point            m_lastRenderBegun;

        PHLMONITORREF              m_monitor;

        UP<Render::ISyncFDManager> m_sync;

        WP<CMonitorFrameScheduler> m_self;

        friend class CMonitor;
    };
}
