#pragma once

#include <aquamarine/backend/Backend.hpp>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "drm-lease-v1.hpp"
#include "../helpers/signal/Signal.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

/*
    TODO: this protocol is not made for systems with multiple DRM nodes (e.g. multigpu)
*/

AQUAMARINE_FORWARD(CDRMBackend);
AQUAMARINE_FORWARD(CDRMLease);
class CDRMLeaseDeviceResource;
class CMonitor;
class CDRMLeaseProtocol;
class CDRMLeaseConnectorResource;
class CDRMLeaseRequestResource;

class CDRMLeaseResource {
  public:
    CDRMLeaseResource(SP<CWpDrmLeaseV1> resource_, SP<CDRMLeaseRequestResource> request);
    ~CDRMLeaseResource();

    bool                                        good();

    WP<CDRMLeaseDeviceResource>                 m_parent;
    std::vector<WP<CDRMLeaseConnectorResource>> m_requested;
    SP<Aquamarine::CDRMLease>                   m_lease;

    struct {
        CHyprSignalListener destroyLease;
    } m_listeners;

  private:
    SP<CWpDrmLeaseV1> m_resource;
};

class CDRMLeaseRequestResource {
  public:
    CDRMLeaseRequestResource(WP<CDRMLeaseDeviceResource> parent_, SP<CWpDrmLeaseRequestV1> resource_);

    bool                                        good();

    WP<CDRMLeaseDeviceResource>                 m_parent;
    WP<CDRMLeaseRequestResource>                m_self;
    std::vector<WP<CDRMLeaseConnectorResource>> m_requested;

  private:
    SP<CWpDrmLeaseRequestV1> m_resource;
};

class CDRMLeaseConnectorResource {
  public:
    CDRMLeaseConnectorResource(WP<CDRMLeaseDeviceResource> parent_, SP<CWpDrmLeaseConnectorV1> resource_, PHLMONITOR monitor_);
    static SP<CDRMLeaseConnectorResource> fromResource(wl_resource*);

    bool                                  good();
    void                                  sendData();

    WP<CDRMLeaseConnectorResource>        m_self;
    WP<CDRMLeaseDeviceResource>           m_parent;
    PHLMONITORREF                         m_monitor;
    bool                                  m_dead = false;

  private:
    SP<CWpDrmLeaseConnectorV1> m_resource;

    struct {
        CHyprSignalListener destroyMonitor;
    } m_listeners;

    friend class CDRMLeaseDeviceResource;
};

class CDRMLeaseDeviceResource {
  public:
    CDRMLeaseDeviceResource(std::string deviceName, SP<CWpDrmLeaseDeviceV1> resource_);

    bool                                        good();
    void                                        sendConnector(PHLMONITOR monitor);

    std::vector<WP<CDRMLeaseConnectorResource>> m_connectorsSent;

    WP<CDRMLeaseDeviceResource>                 m_self;
    std::string                                 m_deviceName;

  private:
    SP<CWpDrmLeaseDeviceV1> m_resource;

    friend class CDRMLeaseProtocol;
};

class CDRMLeaseProtocol : public IWaylandProtocol {
  public:
    CDRMLeaseProtocol(const wl_interface* iface, const int& ver, const std::string& name, SP<Aquamarine::IBackendImplementation> backend);

    virtual void                           bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void                                   offer(PHLMONITOR monitor);

    SP<Aquamarine::IBackendImplementation> getBackend();
    std::string                            getDeviceName();
    bool                                   good();

  private:
    void destroyResource(CDRMLeaseDeviceResource* resource);
    void destroyResource(CDRMLeaseConnectorResource* resource);
    void destroyResource(CDRMLeaseRequestResource* resource);
    void destroyResource(CDRMLeaseResource* resource);

    //
    std::vector<SP<CDRMLeaseDeviceResource>>    m_managers;
    std::vector<SP<CDRMLeaseConnectorResource>> m_connectors;
    std::vector<SP<CDRMLeaseRequestResource>>   m_requests;
    std::vector<SP<CDRMLeaseResource>>          m_leases;

    std::string                                 m_deviceName = "";
    bool                                        m_success    = false;
    SP<Aquamarine::CDRMBackend>                 m_backend;
    std::vector<PHLMONITORREF>                  m_offeredOutputs;

    friend class CDRMLeaseDeviceResource;
    friend class CDRMLeaseConnectorResource;
    friend class CDRMLeaseRequestResource;
    friend class CDRMLeaseResource;
};

namespace PROTO {
    inline std::unordered_map<std::string, SP<CDRMLeaseProtocol>> lease;
};
