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
    void                    onPresented();
    void                    onFrame();

  private:
    bool            canRender();
    void            onFinishRender();
    bool            newSchedulingEnabled();

    bool            m_renderAtFrame = true;
    bool            m_pendingThird  = false;
    hrc::time_point m_lastRenderBegun;

    PHLMONITORREF   m_monitor;
    bool            m_pendingSync = false;
};
