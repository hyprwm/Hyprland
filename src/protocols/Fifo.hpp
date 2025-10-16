#pragma once

#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "fifo-v1.hpp"

#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;

class CFifoResource {
  public:
    CFifoResource(UP<CWpFifoV1>&& resource_, SP<CWLSurfaceResource> surface);
    ~CFifoResource();

    bool good();

  private:
    UP<CWpFifoV1>          m_resource;

    WP<CWLSurfaceResource> m_surface;

    struct SState {
        bool barrierSet    = false;
        bool surfaceLocked = false;
    };

    SState m_pending;

    struct {
        CHyprSignalListener surfacePrecommit;
    } m_listeners;

    void presented();

    friend class CFifoProtocol;
    friend class CFifoManagerResource;
};

class CFifoManagerResource {
  public:
    CFifoManagerResource(UP<CWpFifoManagerV1>&& resource_);
    ~CFifoManagerResource();

    bool good();

  private:
    UP<CWpFifoManagerV1> m_resource;
};

class CFifoProtocol : public IWaylandProtocol {
  public:
    CFifoProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CFifoManagerResource* resource);
    void destroyResource(CFifoResource* resource);

    void onMonitorPresent(PHLMONITOR m);

    //
    std::vector<UP<CFifoManagerResource>> m_managers;
    std::vector<UP<CFifoResource>>        m_fifos;

    friend class CFifoManagerResource;
    friend class CFifoResource;
};

namespace PROTO {
    inline UP<CFifoProtocol> fifo;
};
