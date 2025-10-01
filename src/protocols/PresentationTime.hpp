#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "presentation-time.hpp"
#include "../helpers/time/Time.hpp"

class CMonitor;
class CWLSurfaceResource;

class CQueuedPresentationData {
  public:
    CQueuedPresentationData(SP<CWLSurfaceResource> surf);

    void setPresentationType(bool zeroCopy);
    void attachMonitor(PHLMONITOR pMonitor);

    void presented();
    void discarded();

    bool m_done = false;

  private:
    bool                   m_wasPresented = false;
    bool                   m_zeroCopy     = false;
    PHLMONITORREF          m_monitor;
    WP<CWLSurfaceResource> m_surface;

    friend class CPresentationFeedback;
    friend class CPresentationProtocol;
};

class CPresentationFeedback {
  public:
    CPresentationFeedback(SP<CWpPresentationFeedback> resource_, SP<CWLSurfaceResource> surf);

    bool good();

    void sendQueued(SP<CQueuedPresentationData> data, const Time::steady_tp& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags);

  private:
    SP<CWpPresentationFeedback> m_resource;
    WP<CWLSurfaceResource>      m_surface;
    bool                        m_done = false;

    friend class CPresentationProtocol;
};

class CPresentationProtocol : public IWaylandProtocol {
  public:
    CPresentationProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onPresented(PHLMONITOR pMonitor, const Time::steady_tp& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags);
    void         queueData(SP<CQueuedPresentationData> data);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CPresentationFeedback* feedback);
    void onGetFeedback(CWpPresentation* pMgr, wl_resource* surf, uint32_t id);

    //
    std::vector<UP<CWpPresentation>>         m_managers;
    std::vector<SP<CPresentationFeedback>>   m_feedbacks;
    std::vector<SP<CQueuedPresentationData>> m_queue;

    friend class CPresentationFeedback;
};

namespace PROTO {
    inline UP<CPresentationProtocol> presentation;
};
