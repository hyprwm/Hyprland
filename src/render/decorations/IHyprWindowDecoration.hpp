#pragma once

#include "../../defines.hpp"

enum eDecorationType
{
    DECORATION_NONE = -1,
    DECORATION_GROUPBAR,
    DECORATION_SHADOW,
    DECORATION_CUSTOM
};

struct SWindowDecorationExtents {
    Vector2D topLeft;
    Vector2D bottomRight;
};

class CWindow;
class CMonitor;

interface IHyprWindowDecoration {
  public:
    virtual ~IHyprWindowDecoration() = 0;

    virtual SWindowDecorationExtents getWindowDecorationExtents() = 0;

    virtual void                     draw(CMonitor*, float a, const Vector2D& offset = Vector2D()) = 0;

    virtual eDecorationType          getDecorationType() = 0;

    virtual void                     updateWindow(CWindow*) = 0;

    virtual void                     damageEntire() = 0;

    virtual SWindowDecorationExtents getWindowDecorationReservedArea();

    virtual bool                     allowsInput();
};