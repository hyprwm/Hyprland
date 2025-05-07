#pragma once

#include "../helpers/signal/Signal.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/math/Math.hpp"
#include <vector>

class CWLSurfaceResource;
class CXWaylandSurfaceResource;

#ifdef NO_XWAYLAND
typedef uint32_t xcb_pixmap_t;
typedef uint32_t xcb_window_t;
typedef struct {
    int32_t      flags;
    uint32_t     input;
    int32_t      initial_state;
    xcb_pixmap_t icon_pixmap;
    xcb_window_t icon_window;
    int32_t      icon_x, icon_y;
    xcb_pixmap_t icon_mask;
    xcb_window_t window_group;
} xcb_icccm_wm_hints_t;
typedef struct {
    uint32_t flags;
    int32_t  x, y;
    int32_t  width, height;
    int32_t  min_width, min_height;
    int32_t  max_width, max_height;
    int32_t  width_inc, height_inc;
    int32_t  min_aspect_num, min_aspect_den;
    int32_t  max_aspect_num, max_aspect_den;
    int32_t  base_width, base_height;
    uint32_t win_gravity;
} xcb_size_hints_t;
#else
#include <xcb/xcb_icccm.h>
#endif

class CXWaylandSurface {
  public:
    WP<CWLSurfaceResource>       m_surface;
    WP<CXWaylandSurfaceResource> m_resource;

    struct {
        CSignal stateChanged;    // maximized, fs, minimized, etc.
        CSignal metadataChanged; // title, appid
        CSignal destroy;

        CSignal resourceChange; // associated / dissociated

        CSignal setGeometry;
        CSignal configureRequest; // CBox

        CSignal map;
        CSignal unmap;
        CSignal commit;

        CSignal activate;
    } m_events;

    struct {
        std::string title;
        std::string appid;

        // volatile state: is reset after the stateChanged signal fires
        std::optional<bool> requestsMaximize;
        std::optional<bool> requestsFullscreen;
        std::optional<bool> requestsMinimize;
    } m_state;

    uint32_t                          m_xID         = 0;
    uint64_t                          m_wlID        = 0;
    uint64_t                          m_wlSerial    = 0;
    uint32_t                          m_lastPingSeq = 0;
    pid_t                             m_pid         = 0;
    CBox                              m_geometry;
    bool                              m_overrideRedirect = false;
    bool                              m_withdrawn        = false;
    bool                              m_fullscreen       = false;
    bool                              m_maximized        = false;
    bool                              m_minimized        = false;
    bool                              m_mapped           = false;
    bool                              m_modal            = false;

    WP<CXWaylandSurface>              m_parent;
    WP<CXWaylandSurface>              m_self;
    std::vector<WP<CXWaylandSurface>> m_children;

    UP<xcb_icccm_wm_hints_t>          m_hints;
    UP<xcb_size_hints_t>              m_sizeHints;
    std::vector<uint32_t>             m_atoms;
    std::vector<uint32_t>             m_protocols;
    std::string                       m_role      = "";
    bool                              m_transient = false;

    bool                              wantsFocus();
    void                              configure(const CBox& box);
    void                              activate(bool activate);
    void                              setFullscreen(bool fs);
    void                              setMinimized(bool mz);
    void                              restackToTop();
    void                              close();
    void                              ping();

  private:
    CXWaylandSurface(uint32_t xID, CBox geometry, bool OR);

    void ensureListeners();
    void map();
    void unmap();
    void considerMap();
    void setWithdrawn(bool withdrawn);

    struct {
        CHyprSignalListener destroyResource;
        CHyprSignalListener destroySurface;
        CHyprSignalListener commitSurface;
    } m_listeners;

    friend class CXWM;
};
