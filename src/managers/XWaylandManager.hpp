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
    void                   getGeometryForWindow(PHLWINDOW, CBox*);
    void                   sendCloseWindow(PHLWINDOW);
    void                   setWindowSize(PHLWINDOW, Vector2D, bool force = false);
    void                   setWindowFullscreen(PHLWINDOW, bool);
    bool                   shouldBeFloated(PHLWINDOW, bool pending = false);
    void                   checkBorders(PHLWINDOW);
    Vector2D               getMaxSizeForWindow(PHLWINDOW);
    Vector2D               getMinSizeForWindow(PHLWINDOW);
    Vector2D               xwaylandToWaylandCoords(const Vector2D&);
};

inline std::unique_ptr<CHyprXWaylandManager> g_pXWaylandManager;