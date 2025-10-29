#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <hyprutils/math/Edges.hpp>
#include "WaylandProtocol.hpp"
#include "xdg-shell.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/signal/Signal.hpp"
#include "types/SurfaceRole.hpp"

class CXDGWMBase;
class CXDGPositionerResource;
class CXDGSurfaceResource;
class CXDGToplevelResource;
class CXDGPopupResource;
class CSeatGrab;
class CWLSurfaceResource;
class CXDGDialogV1Resource;

struct SXDGPositionerState {
    Vector2D requestedSize;
    CBox     anchorRect;
    CEdges   anchor;
    CEdges   gravity;
    uint32_t constraintAdjustment = 0;
    Vector2D offset;
    bool     reactive = false;
    Vector2D parentSize;

    void     setAnchor(xdgPositionerAnchor edges);
    void     setGravity(xdgPositionerGravity edges);
};

class CXDGPositionerRules {
  public:
    CXDGPositionerRules(SP<CXDGPositionerResource> positioner);

    CBox getPosition(CBox constraint, const Vector2D& parentPos);

  private:
    SXDGPositionerState m_state;
};

class CXDGPopupResource {
  public:
    CXDGPopupResource(SP<CXdgPopup> resource_, SP<CXDGSurfaceResource> parent_, SP<CXDGSurfaceResource> surface_, SP<CXDGPositionerResource> positioner_);
    ~CXDGPopupResource();

    static SP<CXDGPopupResource> fromResource(wl_resource*);

    bool                         good();

    void                         applyPositioning(const CBox& availableBox, const Vector2D& t1coord /* relative to box */);

    WP<CXDGSurfaceResource>      m_surface;
    WP<CXDGSurfaceResource>      m_parent;
    WP<CXDGPopupResource>        m_self;

    bool                         m_taken = false;

    CBox                         m_geometry;

    struct {
        CSignalT<> reposition;
        CSignalT<> dismissed;
        CSignalT<> destroy; // only the role
    } m_events;

    // schedules a configure event
    void configure(const CBox& box);

    void done();
    void repositioned();

  private:
    SP<CXdgPopup>       m_resource;

    uint32_t            m_lastRepositionToken = 0;

    Vector2D            accumulateParentOffset();

    CXDGPositionerRules m_positionerRules;
};

class CXDGToplevelResource {
  public:
    CXDGToplevelResource(SP<CXdgToplevel> resource_, SP<CXDGSurfaceResource> owner_);
    ~CXDGToplevelResource();

    static SP<CXDGToplevelResource> fromResource(wl_resource*);

    WP<CXDGSurfaceResource>         m_owner;
    WP<CXDGToplevelResource>        m_self;

    PHLWINDOWREF                    m_window;

    bool                            good();

    Vector2D                        layoutMinSize();
    Vector2D                        layoutMaxSize();

    // schedule a configure event
    uint32_t setSize(const Vector2D& size);
    uint32_t setMaximized(bool maximized);
    uint32_t setFullscreen(bool fullscreen);
    uint32_t setActive(bool active);
    uint32_t setSuspeneded(bool sus);

    void     close();

    struct {
        CSignalT<> sizeLimitsChanged;
        CSignalT<> stateChanged;    // maximized, fs, minimized, etc.
        CSignalT<> metadataChanged; // title, appid
        CSignalT<> destroy;         // only the role
    } m_events;

    struct {
        std::string title;
        std::string appid;

        // volatile state: is reset after the stateChanged signal fires
        std::optional<bool>      requestsMaximize;
        std::optional<bool>      requestsFullscreen;
        std::optional<MONITORID> requestsFullscreenMonitor;
        std::optional<bool>      requestsMinimize;
    } m_state;

    struct {
        Vector2D                      size;
        std::vector<xdgToplevelState> states;
    } m_pendingApply;

    struct {
        Vector2D minSize = {1, 1};
        Vector2D maxSize = {1337420, 694200};
    } m_pending, m_current;

    WP<CXDGToplevelResource>              m_parent;
    WP<CXDGDialogV1Resource>              m_dialog;

    std::optional<std::string>            m_toplevelTag;
    std::optional<std::string>            m_toplevelDescription;

    bool                                  anyChildModal();

    std::vector<WP<CXDGToplevelResource>> m_children;

  private:
    SP<CXdgToplevel>     m_resource;

    SP<HOOK_CALLBACK_FN> m_mouseHk, m_touchHk;

    void                 applyState();
};

class CXDGSurfaceRole : public ISurfaceRole {
  public:
    CXDGSurfaceRole(SP<CXDGSurfaceResource> xdg);

    virtual eSurfaceRole role() {
        return SURFACE_ROLE_XDG_SHELL;
    }

    WP<CXDGSurfaceResource> m_xdgSurface;
};

class CXDGSurfaceResource {
  public:
    CXDGSurfaceResource(SP<CXdgSurface> resource_, SP<CXDGWMBase> owner_, SP<CWLSurfaceResource> surface_);
    ~CXDGSurfaceResource();

    static SP<CXDGSurfaceResource> fromResource(wl_resource*);

    bool                           good();

    WP<CXDGWMBase>                 m_owner;
    WP<CWLSurfaceResource>         m_surface;

    WP<CXDGToplevelResource>       m_toplevel;
    WP<CXDGPopupResource>          m_popup;

    WP<CXDGSurfaceResource>        m_self;

    struct {
        CBox geometry;
    } m_pending, m_current;

    struct {
        CSignalT<uint32_t>              ack;
        CSignalT<>                      commit;
        CSignalT<>                      map;
        CSignalT<>                      unmap;
        CSignalT<>                      destroy;
        CSignalT<SP<CXDGPopupResource>> newPopup;
    } m_events;

    bool     m_initialCommit = true;
    bool     m_mapped        = false;

    uint32_t scheduleConfigure();
    // do not call directly
    void configure();

  private:
    SP<CXdgSurface>  m_resource;

    uint32_t         m_lastConfigureSerial = 0;
    uint32_t         m_scheduledSerial     = 0;

    wl_event_source* m_configureSource = nullptr;

    //
    std::vector<WP<CXDGPopupResource>> m_popups;

    struct {
        CHyprSignalListener surfaceDestroy;
        CHyprSignalListener surfaceCommit;
    } m_listeners;

    friend class CXDGPopupResource;
    friend class CXDGToplevelResource;
};

class CXDGPositionerResource {
  public:
    CXDGPositionerResource(SP<CXdgPositioner> resource_, SP<CXDGWMBase> owner_);

    static SP<CXDGPositionerResource> fromResource(wl_resource*);

    bool                              good();

    SXDGPositionerState               m_state;

    WP<CXDGWMBase>                    m_owner;
    WP<CXDGPositionerResource>        m_self;

  private:
    SP<CXdgPositioner> m_resource;
};

class CXDGWMBase {
  public:
    CXDGWMBase(SP<CXdgWmBase> resource_);

    bool                                    good();
    wl_client*                              client();
    void                                    ping();

    std::vector<WP<CXDGPositionerResource>> m_positioners;
    std::vector<WP<CXDGSurfaceResource>>    m_surfaces;

    WP<CXDGWMBase>                          m_self;

    struct {
        CSignalT<> pong;
    } m_events;

  private:
    SP<CXdgWmBase> m_resource;
    wl_client*     m_client = nullptr;
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
    std::vector<SP<CXDGWMBase>>             m_wmBases;
    std::vector<SP<CXDGPositionerResource>> m_positioners;
    std::vector<SP<CXDGSurfaceResource>>    m_surfaces;
    std::vector<SP<CXDGToplevelResource>>   m_toplevels;
    std::vector<SP<CXDGPopupResource>>      m_popups;

    // current popup grab
    WP<CXDGPopupResource>              m_grabOwner;
    SP<CSeatGrab>                      m_grab;
    std::vector<WP<CXDGPopupResource>> m_grabbed;

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
