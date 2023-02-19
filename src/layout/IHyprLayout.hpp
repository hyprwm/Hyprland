#pragma once

#include "../defines.hpp"
#include "../Window.hpp"
#include <any>

struct SWindowRenderLayoutHints {
    bool                isBorderGradient = false;
    CGradientValueData* borderGradient;
};

struct SLayoutMessageHeader {
    CWindow* pWindow = nullptr;
};

enum eFullscreenMode : uint8_t;

enum eRectCorner
{
    CORNER_TOPLEFT = 0,
    CORNER_TOPRIGHT,
    CORNER_BOTTOMRIGHT,
    CORNER_BOTTOMLEFT
};

interface IHyprLayout {
  public:
    virtual ~IHyprLayout()   = 0;
    virtual void onEnable()  = 0;
    virtual void onDisable() = 0;

    /*
        Called when a window is created (mapped)
        The layout HAS TO set the goal pos and size (anim mgr will use it)
        If !animationinprogress, then the anim mgr will not apply an anim.
    */
    virtual void onWindowCreated(CWindow*);
    virtual void onWindowCreatedTiling(CWindow*) = 0;
    virtual void onWindowCreatedFloating(CWindow*);

    /*
        Return tiled status
    */
    virtual bool isWindowTiled(CWindow*) = 0;

    /*
        Called when a window is removed (unmapped)
    */
    virtual void onWindowRemoved(CWindow*);
    virtual void onWindowRemovedTiling(CWindow*) = 0;
    virtual void onWindowRemovedFloating(CWindow*);
    /*
        Called when the monitor requires a layout recalculation
        this usually means reserved area changes
    */
    virtual void recalculateMonitor(const int&) = 0;

    /*
        Called when the compositor requests a window
        to be recalculated, e.g. when pseudo is toggled.
    */
    virtual void recalculateWindow(CWindow*) = 0;

    /*
        Called when a window is requested to be floated
    */
    virtual void changeWindowFloatingMode(CWindow*);
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
    virtual void resizeActiveWindow(const Vector2D&, CWindow* pWindow = nullptr) = 0;
    /*
        Called when a user requests a move of the current window by a vec
        Vector2D holds pixel values
        Optional pWindow for a specific window
    */
    virtual void moveActiveWindow(const Vector2D&, CWindow* pWindow = nullptr);
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
    virtual void fullscreenRequestForWindow(CWindow*, eFullscreenMode, bool) = 0;

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
    virtual SWindowRenderLayoutHints requestRenderHints(CWindow*) = 0;

    /*
        Called when the user requests two windows to be swapped places.
        The layout is free to ignore.
    */
    virtual void switchWindows(CWindow*, CWindow*) = 0;

    /*
        Called when the user requests to change the splitratio by or to X
        on a window
    */
    virtual void alterSplitRatio(CWindow*, float, bool exact = false) = 0;

    /*
        Called when something wants the current layout's name
    */
    virtual std::string getLayoutName() = 0;

    /*
        Called for getting the next candidate for a focus
    */
    virtual CWindow* getNextWindowCandidate(CWindow*);

    /*
        Internal: called when window focus changes
    */
    virtual void onWindowFocusChange(CWindow*);

    /*
        Called for replacing any data a layout has for a new window
    */
    virtual void replaceWindowDataWith(CWindow * from, CWindow * to) = 0;

  private:
    Vector2D    m_vBeginDragXY;
    Vector2D    m_vLastDragXY;
    Vector2D    m_vBeginDragPositionXY;
    Vector2D    m_vBeginDragSizeXY;
    eRectCorner m_eGrabbedCorner = CORNER_TOPLEFT;

    CWindow*    m_pLastTiledWindow = nullptr;
};
