#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "viewporter.hpp"
#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;

class CViewportResource {
  public:
    CViewportResource(SP<CWpViewport> resource_, SP<CWLSurfaceResource> surface_);
    ~CViewportResource();

    bool                   good();
    WP<CWLSurfaceResource> surface;

  private:
    SP<CWpViewport> resource;

    struct {
        CHyprSignalListener surfacePrecommit;
        m_m_listeners;
    };

    class CViewporterResource {
      public:
        CViewporterResource(SP<CWpViewporter> resource_);

        bool good();

      private:
        SP<CWpViewporter> resource;
    };

    class CViewporterProtocol : public IWaylandProtocol {
      public:
        CViewporterProtocol(const wl_interface* iface, const int& ver, const std::string& name);

        virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

      private:
        void destroyResource(CViewporterResource* resource);
        void destroyResource(CViewportResource* resource);

        //
        std::vector<SP<CViewporterResource>> m_vManagers;
        std::vector<SP<CViewportResource>>   m_vViewports;

        friend class CViewporterResource;
        friend class CViewportResource;
    };

    namespace PROTO {
        inline UP<CViewporterProtocol> viewport;
    };
