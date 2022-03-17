#pragma once

#include "defines.hpp"
#include "events/Events.hpp"


class CWindow {
public:

    DYNLISTENER(commitWindow);
    DYNLISTENER(mapWindow);
    DYNLISTENER(unmapWindow);
    DYNLISTENER(destroyWindow);
    DYNLISTENER(setTitleWindow);
    DYNLISTENER(fullscreenWindow);

    union {
        wlr_xdg_surface* xdg;
        wlr_xwayland_surface* xwayland;
    } m_uSurface;

    // TODO: XWayland

    // this is the position and size of the "bounding box"
    Vector2D            m_vPosition = Vector2D(0,0);
    Vector2D            m_vSize = Vector2D(0,0);

    // this is the position and size of the goal placement
    Vector2D            m_vEffectivePosition = Vector2D(0,0);
    Vector2D            m_vEffectiveSize = Vector2D(0,0);

    // this is the real position and size used to draw the thing
    Vector2D            m_vRealPosition = Vector2D(0,0);
    Vector2D            m_vRealSize = Vector2D(0,0);

    uint64_t        m_iTags = 0;
    bool            m_bIsFloating = false;
    bool            m_bIsFullscreen = false;
    uint64_t        m_iMonitorID = -1;
};