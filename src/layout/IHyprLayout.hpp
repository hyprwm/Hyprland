#pragma once

#include "../defines.hpp"
#include "../Window.hpp"

interface IHyprLayout {
public:

    virtual void        onWindowCreated(CWindow*)           = 0;
    virtual void        onWindowRemoved(CWindow*)           = 0;
    virtual void        recalculateMonitor(const int&)      = 0;

    // Floating windows
    virtual void        changeWindowFloatingMode(CWindow*)  = 0;
    virtual void        onBeginDragWindow()                 = 0;
    virtual void        onMouseMove(const Vector2D&)        = 0;
    virtual void        onWindowCreatedFloating(CWindow*)   = 0;

};