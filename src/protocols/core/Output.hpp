#pragma once

#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"

class CMonitor;
class CWLOutputProtocol;

class CWLOutputResource {
  public:
    CWLOutputResource(SP<CWlOutput> resource_, PHLMONITOR pMonitor);
    static SP<CWLOutputResource> fromResource(wl_resource*);

    bool                         good();
    wl_client*                   client();
    SP<CWlOutput>                getResource();
    void                         updateState();

    PHLMONITORREF                m_monitor;
    WP<CWLOutputProtocol>        m_owner;
    WP<CWLOutputResource>        m_self;

  private:
    SP<CWlOutput> m_resource;
    wl_client*    m_client = nullptr;

    friend class CWLOutputProtocol;
};

class CWLOutputProtocol : public IWaylandProtocol {
  public:
    CWLOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name, PHLMONITOR pMonitor);

    virtual void                       bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
    virtual void                       onDisplayDestroy() override;

    std::vector<SP<CWLOutputResource>> outputResourcesFrom(wl_client* client);
    void                               sendDone();

    PHLMONITORREF                      m_monitor;
    WP<CWLOutputProtocol>              m_self;

    // Self-keepalive held while zombie wl_output resources exist, so the
    // protocol stays alive past the PROTO::outputs erase in monitor.added.
    // wl_global_destroy is deferred until the last zombie is destroyed,
    // closing the race with in-flight client requests during DPMS cycles.
    SP<CWLOutputProtocol>              m_selfKeepalive;

    // will mark the protocol for removal, will be removed when no. of bound outputs is 0 (or when overwritten by a new global)
    void remove();
    bool isDefunct(); // true if above was called

    struct {
        CSignalT<SP<CWLOutputResource>> outputBound;
    } m_events;

  private:
    void destroyResource(CWLOutputResource* resource);

    //
    std::vector<SP<CWLOutputResource>> m_outputs;
    // Per-client wl_output resources moved here on monitor removal so
    // wl_global_destroy doesn't invalidate them mid-flight. The wl_global
    // stays valid (because we hold m_selfKeepalive and skip onDisplayDestroy
    // teardown) and clients can keep using their old wl_output ids safely.
    std::vector<SP<CWLOutputResource>> m_zombieOutputs;

    bool                               m_defunct = false;
    std::string                        m_name    = "";

    struct {
        CHyprSignalListener modeChanged;
    } m_listeners;

    friend class CWLOutputResource;
};

namespace PROTO {
    inline std::unordered_map<std::string, SP<CWLOutputProtocol>> outputs;
};
