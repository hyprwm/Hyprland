#pragma once

#include "../defines.hpp"
#include "../Window.hpp"

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
        Called when a the monitor requires a layout recalculation
        this usually means reserved area changes
    */
    virtual void        recalculateMonitor(const int&)      = 0;

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

};