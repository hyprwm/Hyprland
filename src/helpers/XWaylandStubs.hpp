#pragma once

#include <wayland-server.h>

typedef unsigned int xcb_atom_t;
struct xcb_icccm_wm_hints_t;
typedef struct {
    /** User specified flags */
    uint32_t flags;
    /** User-specified position */
    int32_t x, y;
    /** User-specified size */
    int32_t width, height;
    /** Program-specified minimum size */
    int32_t min_width, min_height;
    /** Program-specified maximum size */
    int32_t max_width, max_height;
    /** Program-specified resize increments */
    int32_t width_inc, height_inc;
    /** Program-specified minimum aspect ratios */
    int32_t min_aspect_num, min_aspect_den;
    /** Program-specified maximum aspect ratios */
    int32_t max_aspect_num, max_aspect_den;
    /** Program-specified base size */
    int32_t base_width, base_height;
    /** Program-specified window gravity */
    uint32_t win_gravity;
} xcb_size_hints_t;
typedef unsigned int xcb_window_t;

typedef enum xcb_stack_mode_t
{
    XCB_STACK_MODE_ABOVE     = 0,
    XCB_STACK_MODE_BELOW     = 1,
    XCB_STACK_MODE_TOP_IF    = 2,
    XCB_STACK_MODE_BOTTOM_IF = 3,
    XCB_STACK_MODE_OPPOSITE  = 4
} xcb_stack_mode_t;

struct wlr_xwayland {
    struct wlr_xwayland_server* server;
    struct wlr_xwm*             xwm;
    struct wlr_xwayland_cursor* cursor;

    const char*                 display_name;

    struct wl_display*          wl_display;
    struct wlr_compositor*      compositor;
    struct wlr_seat*            seat;

    void*                       data;
};

struct wlr_xwayland_surface {
    xcb_window_t                 window_id;
    struct wlr_xwm*              xwm;
    uint32_t                     surface_id;

    struct wl_list               link;
    struct wl_list               stack_link;
    struct wl_list               unpaired_link;

    struct wlr_surface*          surface;
    int16_t                      x, y;
    uint16_t                     width, height;
    uint16_t                     saved_width, saved_height;
    bool                         override_redirect;
    bool                         mapped;

    char*                        title;
    char*                        _class;
    char*                        instance;
    char*                        role;
    char*                        startup_id;
    pid_t                        pid;
    bool                         has_utf8_title;

    struct wl_list               children; // wlr_xwayland_surface::parent_link
    struct wlr_xwayland_surface* parent;
    struct wl_list               parent_link; // wlr_xwayland_surface::children

    xcb_atom_t*                  window_type;
    size_t                       window_type_len;

    xcb_atom_t*                  protocols;
    size_t                       protocols_len;

    uint32_t                     decorations;
    xcb_icccm_wm_hints_t*        hints;
    xcb_size_hints_t*            size_hints;

    bool                         pinging;
    struct wl_event_source*      ping_timer;

    // _NET_WM_STATE
    bool modal;
    bool fullscreen;
    bool maximized_vert, maximized_horz;
    bool minimized;

    bool has_alpha;

    struct {
        struct wl_signal destroy;
        struct wl_signal request_configure;
        struct wl_signal request_move;
        struct wl_signal request_resize;
        struct wl_signal request_minimize;
        struct wl_signal request_maximize;
        struct wl_signal request_fullscreen;
        struct wl_signal request_activate;

        struct wl_signal map;
        struct wl_signal unmap;
        struct wl_signal set_title;
        struct wl_signal set_class;
        struct wl_signal set_role;
        struct wl_signal set_parent;
        struct wl_signal set_pid;
        struct wl_signal set_startup_id;
        struct wl_signal set_window_type;
        struct wl_signal set_hints;
        struct wl_signal set_decorations;
        struct wl_signal set_override_redirect;
        struct wl_signal set_geometry;
        struct wl_signal ping_timeout;
    } events;
};

struct wlr_xwayland_surface_configure_event {
    struct wlr_xwayland_surface* surface;
    int16_t                      x, y;
    uint16_t                     width, height;
    uint16_t                     mask; // xcb_config_window_t
};

struct wlr_xwayland_minimize_event {
    struct wlr_xwayland_surface* surface;
    bool                         minimize;
};

inline void wlr_xwayland_destroy(wlr_xwayland*) {}

inline void wlr_xwayland_surface_configure(wlr_xwayland_surface*, int, int, int, int) {}

inline bool wlr_surface_is_xwayland_surface(void*) {
    return false;
}

inline void                  wlr_xwayland_surface_activate(wlr_xwayland_surface*, bool) {}

inline void                  wlr_xwayland_surface_restack(wlr_xwayland_surface*, void*, xcb_stack_mode_t) {}

inline wlr_xwayland_surface* wlr_xwayland_surface_from_wlr_surface(void*) {
    return nullptr;
}

inline void                  wlr_xwayland_surface_close(wlr_xwayland_surface*) {}

inline void                  wlr_xwayland_surface_set_fullscreen(wlr_xwayland_surface*, bool) {}

inline void                  wlr_xwayland_surface_set_minimized(wlr_xwayland_surface*, bool) {}

inline wlr_xwayland_surface* wlr_xwayland_surface_try_from_wlr_surface(wlr_surface*) {
    return nullptr;
}

inline bool wlr_xwayland_or_surface_wants_focus(const wlr_xwayland_surface*) {
    return false;
}