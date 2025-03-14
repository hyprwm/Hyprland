#pragma once

/*
    Implementations for:
     - wl_compositor
     - wl_surface
     - wl_region
     - wl_callback
*/

#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/math/Math.hpp"
#include "../types/Buffer.hpp"
#include "../types/SurfaceRole.hpp"
#include "../types/SurfaceState.hpp"

class CWLOutputResource;
class CMonitor;
class CWLSurface;
class CWLSurfaceResource;
class CWLSubsurfaceResource;
class CViewportResource;
class CDRMSyncobjSurfaceResource;
class CColorManagementSurface;
class CFrogColorManagementSurface;
class CContentType;

class CWLCallbackResource {
  public:
    CWLCallbackResource(SP<CWlCallback> resource_);

    bool good();
    void send(timespec* now);

  private:
    SP<CWlCallback> resource;
};

class CWLRegionResource {
  public:
    CWLRegionResource(SP<CWlRegion> resource_);
    static SP<CWLRegionResource> fromResource(wl_resource* res);

    bool                         good();

    CRegion                      region;
    WP<CWLRegionResource>        self;

  private:
    SP<CWlRegion> resource;
};

class CWLSurfaceResource {
  public:
    CWLSurfaceResource(SP<CWlSurface> resource_);
    ~CWLSurfaceResource();

    static SP<CWLSurfaceResource> fromResource(wl_resource* res);

    bool                          good();
    wl_client*                    client();
    void                          enter(PHLMONITOR monitor);
    void                          leave(PHLMONITOR monitor);
    void                          sendPreferredTransform(wl_output_transform t);
    void                          sendPreferredScale(int32_t scale);
    void                          frame(timespec* now);
    uint32_t                      id();
    void                          map();
    void                          unmap();
    void                          error(int code, const std::string& str);
    SP<CWlSurface>                getResource();
    CBox                          extends();
    void                          resetRole();
    Vector2D                      sourceSize();

    struct {
        CSignal precommit; // before commit
        CSignal commit;    // after commit
        CSignal map;
        CSignal unmap;
        CSignal newSubsurface;
        CSignal destroy;
    } events;

    SSurfaceState                          current, pending;

    std::vector<SP<CWLCallbackResource>>   callbacks;
    WP<CWLSurfaceResource>                 self;
    WP<CWLSurface>                         hlSurface;
    std::vector<PHLMONITORREF>             enteredOutputs;
    bool                                   mapped = false;
    std::vector<WP<CWLSubsurfaceResource>> subsurfaces;
    SP<ISurfaceRole>                       role;
    WP<CViewportResource>                  viewportResource;
    WP<CDRMSyncobjSurfaceResource>         syncobj; // may not be present
    WP<CColorManagementSurface>            colorManagement;
    WP<CContentType>                       contentType;

    void                                   breadthfirst(std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data);
    SP<CWLSurfaceResource>                 findFirstPreorder(std::function<bool(SP<CWLSurfaceResource>)> fn);
    CRegion                                accumulateCurrentBufferDamage();
    void                                   presentFeedback(timespec* when, PHLMONITOR pMonitor, bool discarded = false);
    void                                   commitPendingState(SSurfaceState& state);

    // returns a pair: found surface (null if not found) and surface local coords.
    // localCoords param is relative to 0,0 of this surface
    std::pair<SP<CWLSurfaceResource>, Vector2D> at(const Vector2D& localCoords, bool allowsInput = false);

  private:
    SP<CWlSurface>         resource;
    wl_client*             pClient = nullptr;

    void                   destroy();
    void                   releaseBuffers(bool onlyCurrent = true);
    void                   dropPendingBuffer();
    void                   dropCurrentBuffer();
    void                   bfHelper(std::vector<SP<CWLSurfaceResource>> const& nodes, std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data);
    SP<CWLSurfaceResource> findFirstPreorderHelper(SP<CWLSurfaceResource> root, std::function<bool(SP<CWLSurfaceResource>)> fn);
    void                   updateCursorShm(CRegion damage = CBox{0, 0, INT16_MAX, INT16_MAX});

    friend class CWLPointerResource;
};

class CWLCompositorResource {
  public:
    CWLCompositorResource(SP<CWlCompositor> resource_);

    bool good();

  private:
    SP<CWlCompositor> resource;
};

class CWLCompositorProtocol : public IWaylandProtocol {
  public:
    CWLCompositorProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         forEachSurface(std::function<void(SP<CWLSurfaceResource>)> fn);

    struct {
        CSignal newSurface; // SP<CWLSurfaceResource>
    } events;

  private:
    void destroyResource(CWLCompositorResource* resource);
    void destroyResource(CWLSurfaceResource* resource);
    void destroyResource(CWLRegionResource* resource);

    //
    std::vector<SP<CWLCompositorResource>> m_vManagers;
    std::vector<SP<CWLSurfaceResource>>    m_vSurfaces;
    std::vector<SP<CWLRegionResource>>     m_vRegions;

    friend class CWLSurfaceResource;
    friend class CWLCompositorResource;
    friend class CWLRegionResource;
    friend class CWLCallbackResource;
};

namespace PROTO {
    inline UP<CWLCompositorProtocol> compositor;
};
