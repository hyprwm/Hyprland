#pragma once

#include "../defines.hpp"
#include "../Window.hpp"

class CHyprXWaylandManager {
  public:
    CHyprXWaylandManager();
    ~CHyprXWaylandManager();

    wlr_xwayland* m_sWLRXWayland = nullptr;

    wlr_surface*  getWindowSurface(CWindow*);
    void          activateSurface(wlr_surface*, bool);
    void          activateWindow(CWindow*, bool);
    void          getGeometryForWindow(CWindow*, wlr_box*);
    std::string   getTitle(CWindow*);
    std::string   getAppIDClass(CWindow*);
    void          sendCloseWindow(CWindow*);
    void          setWindowSize(CWindow*, Vector2D, bool force = false);
    void          setWindowStyleTiled(CWindow*, uint32_t);
    void          setWindowFullscreen(CWindow*, bool);
    wlr_surface*  surfaceAt(CWindow*, const Vector2D&, Vector2D&);
    bool          shouldBeFloated(CWindow*);
    void          moveXWaylandWindow(CWindow*, const Vector2D&);
    void          checkBorders(CWindow*);
    Vector2D      getMaxSizeForWindow(CWindow*);
};

inline std::unique_ptr<CHyprXWaylandManager> g_pXWaylandManager;