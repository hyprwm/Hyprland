#pragma once

#include "../defines.hpp"
#include "../Window.hpp"

class CHyprXWaylandManager {
public:
    CHyprXWaylandManager();
    ~CHyprXWaylandManager();

    wlr_xwayland*       m_sWLRXWayland;

    wlr_surface*        getWindowSurface(CWindow*);
    void                activateSurface(wlr_surface*, bool);
    void                getGeometryForWindow(CWindow*, wlr_box*);
    std::string         getTitle(CWindow*);
    void                sendCloseWindow(CWindow*);
    void                setWindowSize(CWindow*, const Vector2D&);
    void                setWindowStyleTiled(CWindow*, uint32_t);
    wlr_surface*        surfaceAt(CWindow*, const Vector2D&, Vector2D&);
    bool                shouldBeFloated(CWindow*);
};

inline std::unique_ptr<CHyprXWaylandManager> g_pXWaylandManager;