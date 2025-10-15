#pragma once

/*
    Implementations for:
     - wl_compositor
     - wl_surface
     - wl_region
     - wl_callback
*/

#include <vector>
#include <queue>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include "../../render/Texture.hpp"
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/math/Math.hpp"
#include "../../helpers/time/Time.hpp"
#include "../types/Buffer.hpp"
#include "../types/ColorManagement.hpp"
#include "../types/SurfaceRole.hpp"
#include "../types/SurfaceState.hpp"

class CWLOutputResource;
class CMonitor;
class CWLSurface;
class CWLSurfaceResource;
class CWLSubsurfaceResource;
class CViewportResource;
class CDRMSyncobjSurfaceResource;
class CFifoResource;
class CColorManagementSurface;
class CFrogColorManagementSurface;
class CContentType;

class CWLCallbackResource {
  public:
    CWLCallbackResource(SP<CWlCallback>&& resource_);
    ~CWLCallbackResource() noexcept = default;
    // disable copy
    CWLCallbackResource(const CWLCallbackResource&)            = delete;
    CWLCallbackResource& operator=(const CWLCallbackResource&) = delete;

    // allow move
    CWLCallbackResource(CWLCallbackResource&&) noexcept            = default;
    CWLCallbackResource& operator=(CWLCallbackResource&&) noexcept = default;

    bool                 good();
    void                 send(const Time::steady_tp& now);

  private:
    SP<CWlCallback> m_resource;
};

class CWLRegionResource {
  public:
    CWLRegionResource(SP<CWlRegion> resource_);
    static SP<CWLRegionResource> fromResource(wl_resource* res);

    bool                         good();

    CRegion                      m_region;
    WP<CWLRegionResource>        m_self;

  private:
    SP<CWlRegion> m_resource;
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
    void                          frame(const Time::steady_tp& now);
    uint32_t                      id();
    void                          map();
    void                          unmap();
    void                          error(int code, const std::string& str);
    SP<CWlSurface>                getResource();
    CBox                          extends();
    void                          resetRole();

    struct {
        CSignalT<>                          precommit; // before commit
        CSignalT<>                          commit;    // after commit
        CSignalT<>                          map;
        CSignalT<>                          unmap;
        CSignalT<SP<CWLSubsurfaceResource>> newSubsurface;
        CSignalT<>                          destroy;
        CSignalT<SP<CMonitor>>              enter;
        CSignalT<SP<CMonitor>>              leave;
    } m_events;

    SSurfaceState                          m_current;
    SSurfaceState                          m_pending;
    std::queue<UP<SSurfaceState>>          m_pendingStates;
    bool                                   m_pendingWaiting = false;
    bool                                   m_fifoLocked     = false;

    WP<CWLSurfaceResource>                 m_self;
    WP<CWLSurface>                         m_hlSurface;
    std::vector<PHLMONITORREF>             m_enteredOutputs;
    bool                                   m_mapped = false;
    std::vector<WP<CWLSubsurfaceResource>> m_subsurfaces;
    SP<ISurfaceRole>                       m_role;
    WP<CDRMSyncobjSurfaceResource>         m_syncobj; // may not be present
    WP<CFifoResource>                      m_fifo;    // may not be present
    WP<CColorManagementSurface>            m_colorManagement;
    WP<CContentType>                       m_contentType;

    void                                   breadthfirst(std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data);
    SP<CWLSurfaceResource>                 findFirstPreorder(std::function<bool(SP<CWLSurfaceResource>)> fn);
    SP<CWLSurfaceResource>                 findWithCM();
    void                                   presentFeedback(const Time::steady_tp& when, PHLMONITOR pMonitor, bool discarded = false);
    void                                   scheduleState(WP<SSurfaceState> state);
    void                                   commitState(SSurfaceState& state);
    NColorManagement::SImageDescription    getPreferredImageDescription();
    void                                   sortSubsurfaces();

    // returns a pair: found surface (null if not found) and surface local coords.
    // localCoords param is relative to 0,0 of this surface
    std::pair<SP<CWLSurfaceResource>, Vector2D> at(const Vector2D& localCoords, bool allowsInput = false);

  private:
    SP<CWlSurface>         m_resource;
    wl_client*             m_client = nullptr;

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
    SP<CWlCompositor> m_resource;
};

class CWLCompositorProtocol : public IWaylandProtocol {
  public:
    CWLCompositorProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         forEachSurface(std::function<void(SP<CWLSurfaceResource>)> fn);

    struct {
        CSignalT<SP<CWLSurfaceResource>> newSurface;
    } m_events;

  private:
    void destroyResource(CWLCompositorResource* resource);
    void destroyResource(CWLSurfaceResource* resource);
    void destroyResource(CWLRegionResource* resource);

    //
    std::vector<SP<CWLCompositorResource>> m_managers;
    std::vector<SP<CWLSurfaceResource>>    m_surfaces;
    std::vector<SP<CWLRegionResource>>     m_regions;

    friend class CWLSurfaceResource;
    friend class CWLCompositorResource;
    friend class CWLRegionResource;
    friend class CWLCallbackResource;
};

namespace PROTO {
    inline UP<CWLCompositorProtocol> compositor;
};
