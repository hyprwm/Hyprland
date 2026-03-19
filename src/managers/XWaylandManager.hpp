#pragma once

#include "../defines.hpp"
#include "../desktop/DesktopTypes.hpp"
#include <optional>

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
};

inline UP<CHyprXWaylandManager> g_pXWaylandManager;
