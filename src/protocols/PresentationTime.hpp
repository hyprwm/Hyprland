#pragma once

#include <ctime>
#include <optional>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "presentation-time.hpp"
#include "../helpers/time/Time.hpp"

class CWLSurfaceResource;
class CPresentationFeedback;

class CQueuedPresentationData {
  public:
    CQueuedPresentationData(SP<CWLSurfaceResource> surf, std::vector<WP<CPresentationFeedback>> feedbacks);

    void setPresentationType(bool zeroCopy);
    void attachMonitor(PHLMONITOR pMonitor);
    void setCommitInfo(uint64_t commitID, bool tearing, bool vrr);

    void presented();
    void discarded();

  private:
    bool                                   m_wasPresented = false;
    bool                                   m_zeroCopy     = false;
    bool                                   m_tearing      = false;
    bool                                   m_vrr          = false;
    std::optional<uint64_t>                m_commitID;
    PHLMONITORREF                          m_monitor;
    WP<CWLSurfaceResource>                 m_surface;
    std::vector<WP<CPresentationFeedback>> m_feedbacks;

    friend class CPresentationFeedback;
    friend class CPresentationProtocol;
};

class CPresentationFeedback {
  public:
    CPresentationFeedback(UP<CWpPresentationFeedback>&& resource_, SP<CWLSurfaceResource> surf);

    bool good();

    void sendQueued(WP<CQueuedPresentationData> data, const timespec& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags);
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

    void onPresented(PHLMONITOR pMonitor, const timespec& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags, uint64_t commitID = 0, bool presented = true);
    void queueData(UP<CQueuedPresentationData>&& data);
    void tagQueued(PHLMONITOR monitor, uint64_t commitID, bool tearing, bool vrr);
    void discardQueued(PHLMONITOR monitor, uint64_t commitID);
    void discardUntagged(PHLMONITOR monitor);
    void discardFeedbacks(std::vector<WP<CPresentationFeedback>>& feedbacks);
    void discardFeedbacksForSurface(WP<CWLSurfaceResource> surface);
    bool hasPendingFeedbacks() const;

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
