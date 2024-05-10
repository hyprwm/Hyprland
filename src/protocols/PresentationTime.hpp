#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "presentation-time.hpp"

class CMonitor;

class CQueuedPresentationData {
  public:
    CQueuedPresentationData(wlr_surface* surf);

    void setPresentationType(bool zeroCopy);
    void attachMonitor(CMonitor* pMonitor);

    void presented();
    void discarded();

  private:
    bool         wasPresented = false;
    bool         zeroCopy     = false;
    CMonitor*    pMonitor     = nullptr;
    wlr_surface* surface      = nullptr; // READ-ONLY

    DYNLISTENER(destroySurface);

    friend class CPresentationFeedback;
    friend class CPresentationProtocol;
};

class CPresentationFeedback {
  public:
    CPresentationFeedback(SP<CWpPresentationFeedback> resource_, wlr_surface* surf);

    bool good();

    void sendQueued(SP<CQueuedPresentationData> data, timespec* when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags);

  private:
    SP<CWpPresentationFeedback> resource;
    wlr_surface*                surface = nullptr; // READ-ONLY
    bool                        done    = false;

    friend class CPresentationProtocol;
};

class CPresentationProtocol : public IWaylandProtocol {
  public:
    CPresentationProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onPresented(CMonitor* pMonitor, timespec* when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags);
    void         queueData(SP<CQueuedPresentationData> data);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CPresentationFeedback* feedback);
    void onGetFeedback(CWpPresentation* pMgr, wl_resource* surf, uint32_t id);

    //
    std::vector<UP<CWpPresentation>>         m_vManagers;
    std::vector<SP<CPresentationFeedback>>   m_vFeedbacks;
    std::vector<SP<CQueuedPresentationData>> m_vQueue;

    friend class CPresentationFeedback;
};

namespace PROTO {
    inline UP<CPresentationProtocol> presentation;
};
