#pragma once

#include "../defines.hpp"
#include "protocols/core/Compositor.hpp"
#include "protocols/types/Buffer.hpp"

class CSurfaceManager {
  public:
    CSurfaceManager() = default;
    bool addBuffer(PHLMONITORREF monitor, const CHLBufferReference& buf);
    bool addFrameCallback(WP<CWLSurfaceResource>, SP<CWLCallbackResource>);
    void addFence(PHLMONITORREF monitor);
    void dropBuffers(PHLMONITORREF monitor);
    void sendFrameCallbacks(WP<CWLSurfaceResource> surf, const Time::steady_tp& now);
    void scheduleForFrame(PHLMONITORREF monitor, WP<CWLSurfaceResource> surf);
    void sendScheduledFrames(PHLMONITORREF monitor, const Time::steady_tp& now);
    void destroy(PHLMONITORREF monitor);

  private:
    std::unordered_map<PHLMONITORREF, std::vector<CHLBufferReference>>               m_buffers;
    std::unordered_map<WP<CWLSurfaceResource>, std::vector<SP<CWLCallbackResource>>> m_frameCallbacks;
    std::unordered_map<PHLMONITORREF, std::vector<WP<CWLSurfaceResource>>>           m_scheduledForFrame;
};

inline UP<CSurfaceManager> g_pSurfaceManager;
