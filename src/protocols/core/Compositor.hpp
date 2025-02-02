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
        CSignal precommit;  // before commit
        CSignal roleCommit; // commit for role objects, before regular commit
        CSignal commit;     // after commit
        CSignal map;
        CSignal unmap;
        CSignal newSubsurface;
        CSignal destroy;
    } events;

    struct SState {
        CRegion                opaque, input = CBox{{}, {INT32_MAX, INT32_MAX}}, damage, bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}} /* initial damage */;
        wl_output_transform    transform = WL_OUTPUT_TRANSFORM_NORMAL;
        int                    scale     = 1;
        SP<CHLBufferReference> buffer; // buffer ref will be released once the buffer is no longer locked. For checking if a buffer is attached to this state, check texture.
        SP<CTexture>           texture;
        Vector2D               offset;
        Vector2D               size, bufferSize;
        struct {
            bool     hasDestination = false;
            bool     hasSource      = false;
            Vector2D destination;
            CBox     source;
        } viewport;
        bool rejected  = false;
        bool newBuffer = false;

        //
        void reset() {
            damage.clear();
            bufferDamage.clear();
            transform = WL_OUTPUT_TRANSFORM_NORMAL;
            scale     = 1;
            offset    = {};
            size      = {};
        }
    } current, pending;

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
    CRegion                                accumulateCurrentBufferDamage();
    void                                   presentFeedback(timespec* when, PHLMONITOR pMonitor, bool discarded = false);
    void                                   lockPendingState();
    void                                   unlockPendingState();

    // returns a pair: found surface (null if not found) and surface local coords.
    // localCoords param is relative to 0,0 of this surface
    std::pair<SP<CWLSurfaceResource>, Vector2D> at(const Vector2D& localCoords, bool allowsInput = false);

  private:
    SP<CWlSurface> resource;
    wl_client*     pClient = nullptr;

    // this is for cursor dumb copy. Due to our (and wayland's...) architecture,
    // this stupid-ass hack is used
    WP<IHLBuffer> lastBuffer;

    int           stateLocks = 0;

    void          destroy();
    void          releaseBuffers(bool onlyCurrent = true);
    void          dropPendingBuffer();
    void          dropCurrentBuffer();
    void          commitPendingState();
    void          bfHelper(std::vector<SP<CWLSurfaceResource>> const& nodes, std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data);
    void          updateCursorShm(CRegion damage = CBox{0, 0, INT16_MAX, INT16_MAX});

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
