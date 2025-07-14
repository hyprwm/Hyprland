#pragma once

#include "../defines.hpp"
#include <any>

class CWindow;
class CGradientValueData;

struct SWindowRenderLayoutHints {
    bool                isBorderGradient = false;
    CGradientValueData* borderGradient   = nullptr;
};

struct SLayoutMessageHeader {
    PHLWINDOW pWindow;
};

enum eFullscreenMode : int8_t;

enum eRectCorner : uint8_t {
    CORNER_NONE        = 0,
    CORNER_TOPLEFT     = (1 << 0),
    CORNER_TOPRIGHT    = (1 << 1),
    CORNER_BOTTOMRIGHT = (1 << 2),
    CORNER_BOTTOMLEFT  = (1 << 3),
};

enum eSnapEdge : uint8_t {
    SNAP_INVALID = 0,
    SNAP_UP      = (1 << 0),
    SNAP_DOWN    = (1 << 1),
    SNAP_LEFT    = (1 << 2),
    SNAP_RIGHT   = (1 << 3),
};

enum eDirection : int8_t {
    DIRECTION_DEFAULT = -1,
    DIRECTION_UP      = 0,
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT
};

class IHyprLayout {
  public:
    virtual ~IHyprLayout()   = default;
    virtual void onEnable()  = 0;
    virtual void onDisable() = 0;

    /*
        Called when a window is created (mapped)
        The layout HAS TO set the goal pos and size (anim mgr will use it)
        If !animationinprogress, then the anim mgr will not apply an anim.
    */
    virtual void onWindowCreated(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT);
    virtual void onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT) = 0;
    virtual void onWindowCreatedFloating(PHLWINDOW);
    virtual bool onWindowCreatedAutoGroup(PHLWINDOW);

    /*
        Return tiled status
    */
    virtual bool isWindowTiled(PHLWINDOW) = 0;

    /*
        Called when a window is removed (unmapped)
    */
    virtual void onWindowRemoved(PHLWINDOW);
    virtual void onWindowRemovedTiling(PHLWINDOW) = 0;
    virtual void onWindowRemovedFloating(PHLWINDOW);
    /*
        Called when the monitor requires a layout recalculation
        this usually means reserved area changes
    */
    virtual void recalculateMonitor(const MONITORID&) = 0;

    /*
        Called when the compositor requests a window
        to be recalculated, e.g. when pseudo is toggled.
    */
    virtual void recalculateWindow(PHLWINDOW) = 0;

    /*
        Called when a window is requested to be floated
    */
    virtual void changeWindowFloatingMode(PHLWINDOW);
    /*
        Called when a window is clicked on, beginning a drag
        this might be a resize, move, whatever the layout defines it
        as.
    */
    virtual void onBeginDragWindow();
    /*
        Called when a user requests a resize of the current window by a vec
        Vector2D holds pixel values
        Optional pWindow for a specific window
    */
    virtual void resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, PHLWINDOW pWindow = nullptr) = 0;
    /*
        Called when a user requests a move of the current window by a vec
        Vector2D holds pixel values
        Optional pWindow for a specific window
    */
    virtual void moveActiveWindow(const Vector2D&, PHLWINDOW pWindow = nullptr);
    /*
        Called when a window is ended being dragged
        (mouse up)
    */
    virtual void onEndDragWindow();
    /*
        Called whenever the mouse moves, should the layout want to
        do anything with it.
        Useful for dragging.
    */
    virtual void onMouseMove(const Vector2D&);

    /*
        Called when a window / the user requests to toggle the fullscreen state of a window
        The layout sets all the fullscreen flags.
        It can either accept or ignore.
    */
    virtual void fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) = 0;

    /*
        Called when a dispatcher requests a custom message
        The layout is free to ignore.
        std::any is the reply. Can be empty.
    */
    virtual std::any layoutMessage(SLayoutMessageHeader, std::string) = 0;

    /*
        Required to be handled, but may return just SWindowRenderLayoutHints()
        Called when the renderer requests any special draw flags for
        a specific window, e.g. border color for groups.
    */
    virtual SWindowRenderLayoutHints requestRenderHints(PHLWINDOW) = 0;

    /*
        Called when the user requests two windows to be swapped places.
        The layout is free to ignore.
    */
    virtual void switchWindows(PHLWINDOW, PHLWINDOW) = 0;

    /*
        Called when the user requests a window move in a direction.
        The layout is free to ignore.
    */
    virtual void moveWindowTo(PHLWINDOW, const std::string& direction, bool silent = false) = 0;

    /*
        Called when the user requests to change the splitratio by or to X
        on a window
    */
    virtual void alterSplitRatio(PHLWINDOW, float, bool exact = false) = 0;

    /*
        Called when something wants the current layout's name
    */
    virtual std::string getLayoutName() = 0;

    /*
        Called for getting the next candidate for a focus
    */
    virtual PHLWINDOW getNextWindowCandidate(PHLWINDOW);

    /*
        Internal: called when window focus changes
    */
    virtual void onWindowFocusChange(PHLWINDOW);

    /*
        Called for replacing any data a layout has for a new window
    */
    virtual void replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) = 0;

    /*
        Determines if a window can be focused. If hidden this usually means the window is part of a group.
    */
    virtual bool isWindowReachable(PHLWINDOW);

    /*
        Called before an attempt is made to focus a window.
        Brings the window to the top of any groups and ensures it is not hidden.
        If the window is unmapped following this call, the focus attempt will fail.
    */
    virtual void bringWindowToTop(PHLWINDOW);

    /*
        Called via the foreign toplevel activation protocol.
        Focuses a window, bringing it to the top of its group if applicable.
        May be ignored.
    */
    virtual void requestFocusForWindow(PHLWINDOW);

    /*
        Called to predict the size of a newly opened window to send it a configure.
        Return 0,0 if unpredictable
    */
    virtual Vector2D predictSizeForNewWindowTiled() = 0;

    /*
        Prefer not overriding, use predictSizeForNewWindowTiled.
    */
    virtual Vector2D predictSizeForNewWindow(PHLWINDOW pWindow);
    virtual Vector2D predictSizeForNewWindowFloating(PHLWINDOW pWindow);

    /*
        Called to try to pick up window for dragging.
        Updates drag related variables and floats window if threshold reached.
        Return true to reject
    */
    virtual bool updateDragWindow();

  private:
    int          m_mouseMoveEventCount;
    Vector2D     m_beginDragXY;
    Vector2D     m_lastDragXY;
    Vector2D     m_beginDragPositionXY;
    Vector2D     m_beginDragSizeXY;
    Vector2D     m_draggingWindowOriginalFloatSize;
    eRectCorner  m_grabbedCorner = CORNER_TOPLEFT;
    void         handleDragThreshold(const Vector2D& mousePos, const SP<CWindow>& window);
    bool         shouldUpdateMouseMove(const Vector2D& mousePos);
    void         updateWindowPosition(const Vector2D& mousePos, const SP<CWindow>& win);
    void         updateWindowResize(const Vector2D& mousePos, const SP<CWindow>& win);
    void         handleFloatingResize(const Vector2D& mousePos, const SP<CWindow>& win);
    Vector2D     computeNewResizeSize(const Vector2D& delta);
    Vector2D     computeNewResizePos(const Vector2D& newSize);
    void         enforceAspectRatio(Vector2D& size, Vector2D& minSize, Vector2D& maxSize, const SP<CWindow>& win);
    Vector2D     ratioAdjust(const Vector2D& in, float ratio, bool expand);
    void         updateWindowMonitorAndDecos(const SP<CWindow>& win);

    PHLWINDOWREF m_lastTiledWindow;
};
