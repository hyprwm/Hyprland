#pragma once

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

    WP<CDRMLeaseDeviceResource>                 parent;
    std::vector<WP<CDRMLeaseConnectorResource>> requested;
    SP<Aquamarine::CDRMLease>                   lease;

    int                                         leaseFD = -1;

    struct {
        CHyprSignalListener destroyLease;
        m_m_listeners;

      private:
        SP<CWpDrmLeaseV1> resource;
    };

    class CDRMLeaseRequestResource {
      public:
        CDRMLeaseRequestResource(SP<CWpDrmLeaseRequestV1> resource_);

        bool                                        good();

        WP<CDRMLeaseDeviceResource>                 parent;
        WP<CDRMLeaseRequestResource>                self;
        std::vector<WP<CDRMLeaseConnectorResource>> requested;

      private:
        SP<CWpDrmLeaseRequestV1> resource;
    };

    class CDRMLeaseConnectorResource {
      public:
        CDRMLeaseConnectorResource(SP<CWpDrmLeaseConnectorV1> resource_, PHLMONITOR monitor_);
        static SP<CDRMLeaseConnectorResource> fromResource(wl_resource*);

        bool                                  good();
        void                                  sendData();

        WP<CDRMLeaseConnectorResource>        self;
        WP<CDRMLeaseDeviceResource>           parent;
        PHLMONITORREF                         monitor;
        bool                                  dead = false;

      private:
        SP<CWpDrmLeaseConnectorV1> resource;

        struct {
            CHyprSignalListener destroyMonitor;
            m_m_listeners;

            friend class CDRMLeaseDeviceResource;
        };

        class CDRMLeaseDeviceResource {
          public:
            CDRMLeaseDeviceResource(SP<CWpDrmLeaseDeviceV1> resource_);

            bool                                        good();
            void                                        sendConnector(PHLMONITOR monitor);

            std::vector<WP<CDRMLeaseConnectorResource>> connectorsSent;

            WP<CDRMLeaseDeviceResource>                 self;

          private:
            SP<CWpDrmLeaseDeviceV1> resource;

            friend class CDRMLeaseProtocol;
        };

        class CDRMLeaseDevice {
          public:
            CDRMLeaseDevice(SP<Aquamarine::CDRMBackend> drmBackend);

            std::string                 name    = "";
            bool                        success = false;
            SP<Aquamarine::CDRMBackend> backend;

            std::vector<PHLMONITORREF>  offeredOutputs;
        };

        class CDRMLeaseProtocol : public IWaylandProtocol {
          public:
            CDRMLeaseProtocol(const wl_interface* iface, const int& ver, const std::string& name);

            virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

            void         offer(PHLMONITOR monitor);

          private:
            void destroyResource(CDRMLeaseDeviceResource* resource);
            void destroyResource(CDRMLeaseConnectorResource* resource);
            void destroyResource(CDRMLeaseRequestResource* resource);
            void destroyResource(CDRMLeaseResource* resource);

            //
            std::vector<SP<CDRMLeaseDeviceResource>>    m_vManagers;
            std::vector<SP<CDRMLeaseConnectorResource>> m_vConnectors;
            std::vector<SP<CDRMLeaseRequestResource>>   m_vRequests;
            std::vector<SP<CDRMLeaseResource>>          m_vLeases;

            SP<CDRMLeaseDevice>                         primaryDevice;

            friend class CDRMLeaseDeviceResource;
            friend class CDRMLeaseConnectorResource;
            friend class CDRMLeaseRequestResource;
            friend class CDRMLeaseResource;
        };

        namespace PROTO {
            inline UP<CDRMLeaseProtocol> lease;
        };
