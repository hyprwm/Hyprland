#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "color-representation-v1.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/cm/ColorRepresentation.hpp"

class CWLSurfaceResource;

class CColorRepresentationSurface {
  public:
    CColorRepresentationSurface(UP<CWpColorRepresentationSurfaceV1>&& resource, SP<CWLSurfaceResource> surface);

    bool                   good();

    WP<CWLSurfaceResource> m_surface;

  private:
    UP<CWpColorRepresentationSurfaceV1> m_resource;

    void                                destroy();
    void                                setPending(const NColorManagement::SColorRepresentation& representation);
    void                                onPrecommit();

    struct {
        CHyprSignalListener surfacePrecommit;
    } m_listeners;
};

class CColorRepresentationProtocol : public IWaylandProtocol {
  public:
    CColorRepresentationProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                             destroyManager(CWpColorRepresentationManagerV1* manager);
    void                                             destroySurface(CColorRepresentationSurface* surface);
    void                                             getSurface(CWpColorRepresentationManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface);

    std::vector<UP<CWpColorRepresentationManagerV1>> m_managers;
    std::vector<UP<CColorRepresentationSurface>>     m_surfaces;

    friend class CColorRepresentationSurface;
};

namespace PROTO {
    inline UP<CColorRepresentationProtocol> colorRepresentation;
};
