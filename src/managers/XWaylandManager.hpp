#pragma once

#include "../defines.hpp"
#include <optional>

class CWindow; // because clangd
typedef SP<CWindow> PHLWINDOW;
class CWLSurfaceResource;

class CHyprXWaylandManager {
  public:
    CHyprXWaylandManager();
    ~CHyprXWaylandManager();

    SP<CWLSurfaceResource> getWindowSurface(PHLWINDOW);
    void                   activateSurface(SP<CWLSurfaceResource>, bool);
    void                   activateWindow(PHLWINDOW, bool);
    CBox                   getGeometryForWindow(PHLWINDOW);
    void                   sendCloseWindow(PHLWINDOW);
    void                   setWindowFullscreen(PHLWINDOW, bool);
    bool                   shouldBeFloated(PHLWINDOW, bool pending = false);
    void                   checkBorders(PHLWINDOW);
    Vector2D               xwaylandToWaylandCoords(const Vector2D&);
    Vector2D               waylandToXWaylandCoords(const Vector2D&);
};

inline UP<CHyprXWaylandManager> g_pXWaylandManager;