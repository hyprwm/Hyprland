#pragma once

#include "../defines.hpp"
#include "../Window.hpp"

struct SWindowRenderLayoutHints {
    bool        isBorderColor = false;
    CColor      borderColor;
};

interface IHyprLayout {
public:

    /*
        Called when a window is created (mapped)
    */
    virtual void        onWindowCreated(CWindow*)           = 0;
    /*
        Called when a window is removed (unmapped)
    */
    virtual void        onWindowRemoved(CWindow*)           = 0;
    /*
        Called when the monitor requires a layout recalculation
        this usually means reserved area changes
    */
    virtual void        recalculateMonitor(const int&)      = 0;

    /*
        Called when the compositor requests a window
        to be recalculated, e.g. when pseudo is toggled.
    */
    virtual void        recalculateWindow(CWindow*)         = 0;

    /*
        Called when a window is requested to be floated
    */
    virtual void        changeWindowFloatingMode(CWindow*)  = 0;
    /*
        Called when a window is clicked on, beginning a drag
        this might be a resize, move, whatever the layout defines it
        as.
    */
    virtual void        onBeginDragWindow()                 = 0;
    /*
        Called when a window is ended being dragged
        (mouse up)
    */
    virtual void        onEndDragWindow()                   = 0;
    /*
        Called whenever the mouse moves, should the layout want to 
        do anything with it.
        Useful for dragging.
    */
    virtual void        onMouseMove(const Vector2D&)        = 0;
    /*
        Called when a window is created, but is requesting to be floated.
        Warning: this also includes stuff like popups, incorrect handling
        of which can result in a crash!
    */
    virtual void        onWindowCreatedFloating(CWindow*)   = 0;

    /*
        Called when a window requests to toggle its' fullscreen state.
        The layout sets all the fullscreen flags.
        It can either accept or ignore.
    */
    virtual void        fullscreenRequestForWindow(CWindow*)    = 0;

    /*
        Called when the user requests a window to be made into a group,
        or when they want the group to be released.
        Everything else is free to interpret by the layout.
    */
    virtual void         toggleWindowGroup(CWindow*)         = 0;

    /*
        Called when the user requests a group window switch
    */
    virtual void         switchGroupWindow(CWindow*)         = 0;

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
    virtual void         switchWindows(CWindow*, CWindow*)      = 0;

    /*
        Called when the user requests to change the splitratio by X
        on a window
    */
    virtual void         alterSplitRatioBy(CWindow*, float)     = 0;
};