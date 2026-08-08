#pragma once

#include "../defines.hpp"
#include "../desktop/DesktopTypes.hpp"

class CWLSurfaceResource;

class CHyprXWaylandManager {
  public:
    CHyprXWaylandManager();
    ~CHyprXWaylandManager();

    void     activateSurface(SP<CWLSurfaceResource>, bool);
    void     activateWindow(PHLWINDOW, bool);
    Vector2D xwaylandToWaylandCoords(const Vector2D&);
    Vector2D xwaylandToWaylandCoords(const Vector2D&, PHLMONITOR);
    Vector2D waylandToXWaylandCoords(const Vector2D&);
    Vector2D waylandToXWaylandCoords(const Vector2D&, PHLMONITOR);
};

inline UP<CHyprXWaylandManager> g_pXWaylandManager;
