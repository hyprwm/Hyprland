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

    PHLMONITORREF                monitor;
    WP<CWLOutputProtocol>        owner;
    WP<CWLOutputResource>        self;

  private:
    SP<CWlOutput> resource;
    wl_client*    pClient = nullptr;

    friend class CWLOutputProtocol;
};

class CWLOutputProtocol : public IWaylandProtocol {
  public:
    CWLOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name, PHLMONITOR pMonitor);

    virtual void          bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    SP<CWLOutputResource> outputResourceFrom(wl_client* client);
    void                  sendDone();

    PHLMONITORREF         monitor;
    WP<CWLOutputProtocol> self;

    // will mark the protocol for removal, will be removed when no. of bound outputs is 0 (or when overwritten by a new global)
    void remove();
    bool isDefunct(); // true if above was called

  private:
    void destroyResource(CWLOutputResource* resource);

    //
    std::vector<SP<CWLOutputResource>> m_vOutputs;
    bool                               defunct = false;
    std::string                        szName  = "";

    struct {
        CHyprSignalListener modeChanged;
        m_m_listeners;

        friend class CWLOutputResource;
    };

    namespace PROTO {
        inline std::unordered_map<std::string, SP<CWLOutputProtocol>> outputs;
    };
