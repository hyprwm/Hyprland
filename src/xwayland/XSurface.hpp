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
    WP<CWLSurfaceResource>       surface;
    WP<CXWaylandSurfaceResource> resource;

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
    } events;

    struct {
        std::string title;
        std::string appid;

        // volatile state: is reset after the stateChanged signal fires
        std::optional<bool> requestsMaximize;
        std::optional<bool> requestsFullscreen;
        std::optional<bool> requestsMinimize;
    } state;

    uint32_t                          xID         = 0;
    uint64_t                          wlID        = 0;
    uint64_t                          wlSerial    = 0;
    uint32_t                          lastPingSeq = 0;
    pid_t                             pid         = 0;
    CBox                              geometry;
    bool                              overrideRedirect = false;
    bool                              withdrawn        = false;
    bool                              fullscreen       = false;
    bool                              maximized        = false;
    bool                              minimized        = false;
    bool                              mapped           = false;
    bool                              modal            = false;

    WP<CXWaylandSurface>              parent;
    WP<CXWaylandSurface>              self;
    std::vector<WP<CXWaylandSurface>> children;

    UP<xcb_icccm_wm_hints_t>          hints;
    UP<xcb_size_hints_t>              sizeHints;
    std::vector<uint32_t>             atoms, protocols;
    std::string                       role      = "";
    bool                              transient = false;

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

    void ensurem_listeners();
    void map();
    void unmap();
    void considerMap();
    void setWithdrawn(bool withdrawn);

    struct {
        CHyprSignalListener destroyResource;
        CHyprSignalListener destroySurface;
        CHyprSignalListener commitSurface;
        m_m_listeners;

        friend class CXWM;
    };
