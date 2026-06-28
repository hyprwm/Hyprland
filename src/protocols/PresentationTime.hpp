#pragma once

#include <ctime>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "presentation-time.hpp"
#include "../helpers/time/Time.hpp"

class CWLSurfaceResource;
class CPresentationFeedback;

class CQueuedPresentationData {
  public:
    CQueuedPresentationData(SP<CWLSurfaceResource> surf, uint64_t commitSeq);

    void setPresentationType(bool zeroCopy);
    void attachMonitor(PHLMONITOR pMonitor);
    void addFeedback(WP<CPresentationFeedback> feedback);
    void addFeedbacks(std::vector<WP<CPresentationFeedback>>&& feedbacks);

    void presented();
    void discarded();
    bool hasMonitor(PHLMONITOR pMonitor) const;
    void detachMonitor(PHLMONITOR pMonitor);
    bool hasMonitors() const;

    bool m_done = false;

  private:
    bool                                   m_wasPresented = false;
    bool                                   m_zeroCopy     = false;
    uint64_t                               m_commitSeq    = 0;
    std::vector<PHLMONITORREF>             m_monitors;
    WP<CWLSurfaceResource>                 m_surface;
    std::vector<WP<CPresentationFeedback>> m_feedbacks;

    friend class CPresentationFeedback;
    friend class CPresentationProtocol;
};

class CPresentationFeedback {
  public:
    CPresentationFeedback(UP<CWpPresentationFeedback>&& resource_, SP<CWLSurfaceResource> surf);

    bool good();

    void sendQueued(const CQueuedPresentationData& data, PHLMONITOR pMonitor, const timespec& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags);
    void sendDiscarded();

  private:
    UP<CWpPresentationFeedback> m_resource;
    WP<CWLSurfaceResource>      m_surface;
    bool                        m_done = false;

    friend class CPresentationProtocol;
};

class CPresentationProtocol : public IWaylandProtocol {
  public:
    CPresentationProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onPresented(PHLMONITOR pMonitor, const timespec& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags);
    void         queueData(UP<CQueuedPresentationData>&& data);
    bool         addFeedback(WP<CWLSurfaceResource> surf, uint64_t commitSeq, WP<CPresentationFeedback> feedback);
    bool         queueData(WP<CWLSurfaceResource> surf, uint64_t commitSeq, std::vector<WP<CPresentationFeedback>>&& feedbacks, PHLMONITOR pMonitor, bool zeroCopy);
    bool         addPresentedMonitor(WP<CWLSurfaceResource> surf, uint64_t commitSeq, PHLMONITOR pMonitor, bool zeroCopy);
    void         discardFeedbacks(std::vector<WP<CPresentationFeedback>>& feedbacks);
    void         discardSurface(WP<CWLSurfaceResource> surf);
    void         discardPresentedForMonitor(PHLMONITOR pMonitor);
    bool         hasPendingFeedbacks() const;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CPresentationFeedback* feedback);
    void onGetFeedback(CWpPresentation* pMgr, wl_resource* surf, uint32_t id);

    //
    std::vector<UP<CWpPresentation>>         m_managers;
    std::vector<UP<CPresentationFeedback>>   m_feedbacks;
    std::vector<UP<CQueuedPresentationData>> m_queue;

    friend class CPresentationFeedback;
};

namespace PROTO {
    inline UP<CPresentationProtocol> presentation;
};
