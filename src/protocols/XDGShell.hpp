#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <optional>
#include "WaylandProtocol.hpp"
#include "xdg-shell.hpp"
#include "../helpers/Vector2D.hpp"
#include "../helpers/Box.hpp"
#include "../helpers/signal/Signal.hpp"

class CXDGWMBase;
class CXDGPositionerResource;
class CXDGSurfaceResource;
class CXDGToplevelResource;
class CXDGPopupResource;
class CSeatGrab;

struct SXDGPositionerState {
    Vector2D             requestedSize;
    CBox                 anchorRect;
    xdgPositionerAnchor  anchor               = XDG_POSITIONER_ANCHOR_NONE;
    xdgPositionerGravity gravity              = XDG_POSITIONER_GRAVITY_NONE;
    uint32_t             constraintAdjustment = 0;
    Vector2D             offset;
    bool                 reactive = false;
    Vector2D             parentSize;
};

class CXDGPositionerRules {
  public:
    CXDGPositionerRules(SP<CXDGPositionerResource> positioner);

    CBox getPosition(const CBox& constraint, const Vector2D& parentPos);

  private:
    SXDGPositionerState state;
};

class CXDGPopupResource {
  public:
    CXDGPopupResource(SP<CXdgPopup> resource_, SP<CXDGSurfaceResource> parent_, SP<CXDGSurfaceResource> surface_, SP<CXDGPositionerResource> positioner_);
    ~CXDGPopupResource();

    static SP<CXDGPopupResource> fromResource(wl_resource*);

    bool                         good();

    void                         applyPositioning(const CBox& availableBox, const Vector2D& t1coord /* relative to box */);

    WP<CXDGSurfaceResource>      surface;
    WP<CXDGSurfaceResource>      parent;
    WP<CXDGPopupResource>        self;

    bool                         taken = false;

    CBox                         geometry;

    struct {
        CSignal reposition;
        CSignal dismissed;
        CSignal destroy; // only the role
    } events;

    // schedules a configure event
    void configure(const CBox& box);

    void done();
    void repositioned();

  private:
    SP<CXdgPopup>       resource;

    uint32_t            lastRepositionToken = 0;

    Vector2D            accumulateParentOffset();

    CXDGPositionerRules positionerRules;
};

class CXDGToplevelResource {
  public:
    CXDGToplevelResource(SP<CXdgToplevel> resource_, SP<CXDGSurfaceResource> owner_);
    ~CXDGToplevelResource();

    static SP<CXDGToplevelResource> fromResource(wl_resource*);

    WP<CXDGSurfaceResource>         owner;
    WP<CXDGToplevelResource>        self;

    PHLWINDOWREF                    window;

    bool                            good();

    // schedule a configure event
    uint32_t setSize(const Vector2D& size);
    uint32_t setMaximized(bool maximized);
    uint32_t setFullscreen(bool fullscreen);
    uint32_t setActive(bool active);
    uint32_t setSuspeneded(bool sus);

    void     close();

    struct {
        CSignal sizeLimitsChanged;
        CSignal stateChanged;    // maximized, fs, minimized, etc.
        CSignal metadataChanged; // title, appid
        CSignal destroy;         // only the role
    } events;

    struct {
        std::string title;
        std::string appid;

        // volatile state: is reset after the stateChanged signal fires
        std::optional<bool> requestsMaximize;
        std::optional<bool> requestsFullscreen;
        std::optional<bool> requestsMinimize;
    } state;

    struct {
        Vector2D                      size;
        std::vector<xdgToplevelState> states;
    } pendingApply;

    struct {
        Vector2D minSize = {1, 1};
        Vector2D maxSize = {1337420, 694200};
    } pending, current;

    WP<CXDGToplevelResource> parent;

  private:
    SP<CXdgToplevel> resource;
    void             applyState();
};

class CXDGSurfaceResource {
  public:
    CXDGSurfaceResource(SP<CXdgSurface> resource_, SP<CXDGWMBase> owner_, wlr_surface* surface_);
    ~CXDGSurfaceResource();

    static SP<CXDGSurfaceResource> fromResource(wl_resource*);

    bool                           good();

    WP<CXDGWMBase>                 owner;
    wlr_surface*                   surface = nullptr;

    WP<CXDGToplevelResource>       toplevel;
    WP<CXDGPopupResource>          popup;

    WP<CXDGSurfaceResource>        self;

    struct {
        CBox geometry;
    } pending, current;

    struct {
        CSignal ack;
        CSignal commit;
        CSignal map;
        CSignal unmap;
        CSignal destroy;
        CSignal newPopup; // SP<CXDGPopupResource>
    } events;

    bool     initialCommit = true;
    bool     mapped        = false;

    uint32_t scheduleConfigure();
    // do not call directly
    void configure();

  private:
    SP<CXdgSurface>  resource;

    uint32_t         lastConfigureSerial = 0;
    uint32_t         scheduledSerial     = 0;

    wl_event_source* configureSource = nullptr;

    //
    std::vector<WP<CXDGPopupResource>> popups;

    DYNLISTENER(surfaceDestroy);
    DYNLISTENER(surfaceCommit);

    friend class CXDGPopupResource;
    friend class CXDGToplevelResource;
};

class CXDGPositionerResource {
  public:
    CXDGPositionerResource(SP<CXdgPositioner> resource_, SP<CXDGWMBase> owner_);

    static SP<CXDGPositionerResource> fromResource(wl_resource*);

    bool                              good();

    SXDGPositionerState               state;

    WP<CXDGWMBase>                    owner;
    WP<CXDGPositionerResource>        self;

  private:
    SP<CXdgPositioner> resource;
};

class CXDGWMBase {
  public:
    CXDGWMBase(SP<CXdgWmBase> resource_);

    bool                                    good();
    wl_client*                              client();

    std::vector<WP<CXDGPositionerResource>> positioners;
    std::vector<WP<CXDGSurfaceResource>>    surfaces;

    WP<CXDGWMBase>                          self;

  private:
    SP<CXdgWmBase> resource;
    wl_client*     pClient = nullptr;
};

class CXDGShellProtocol : public IWaylandProtocol {
  public:
    CXDGShellProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CXDGWMBase* resource);
    void destroyResource(CXDGPositionerResource* resource);
    void destroyResource(CXDGSurfaceResource* resource);
    void destroyResource(CXDGToplevelResource* resource);
    void destroyResource(CXDGPopupResource* resource);

    //
    std::vector<SP<CXDGWMBase>>             m_vWMBases;
    std::vector<SP<CXDGPositionerResource>> m_vPositioners;
    std::vector<SP<CXDGSurfaceResource>>    m_vSurfaces;
    std::vector<SP<CXDGToplevelResource>>   m_vToplevels;
    std::vector<SP<CXDGPopupResource>>      m_vPopups;

    // current popup grab
    WP<CXDGPopupResource>              grabOwner;
    SP<CSeatGrab>                      grab;
    std::vector<WP<CXDGPopupResource>> grabbed;

    void                               addOrStartGrab(SP<CXDGPopupResource> popup);
    void                               onPopupDestroy(WP<CXDGPopupResource> popup);

    friend class CXDGWMBase;
    friend class CXDGPositionerResource;
    friend class CXDGSurfaceResource;
    friend class CXDGToplevelResource;
    friend class CXDGPopupResource;
};

namespace PROTO {
    inline UP<CXDGShellProtocol> xdgShell;
};
