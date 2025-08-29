#pragma once

#include "Monitor.hpp"

#include <chrono>

class CEGLSync;

class CMonitorFrameScheduler {
  public:
    using hrc = std::chrono::high_resolution_clock;

    CMonitorFrameScheduler(PHLMONITOR m);

    CMonitorFrameScheduler(const CMonitorFrameScheduler&)            = delete;
    CMonitorFrameScheduler(CMonitorFrameScheduler&&)                 = delete;
    CMonitorFrameScheduler& operator=(const CMonitorFrameScheduler&) = delete;
    CMonitorFrameScheduler& operator=(CMonitorFrameScheduler&&)      = delete;

    void                    onSyncFired();
    void                    onFrame();

  private:
    bool                       canRender();
    void                       onFinishRender(Hyprutils::OS::CFileDescriptor syncFd = {});
    bool                       newSchedulingEnabled();

    bool                       m_renderAtFrame = true;
    bool                       m_pendingSync   = false;
    hrc::time_point            m_lastRenderBegun;

    PHLMONITORREF              m_monitor;

    WP<CMonitorFrameScheduler> m_self;

    friend class CMonitor;
};
