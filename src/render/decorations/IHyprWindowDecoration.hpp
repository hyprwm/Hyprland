#pragma once

#include "../../defines.hpp"
#include "../../helpers/Region.hpp"

enum eDecorationType {
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

class IHyprWindowDecoration {
  public:
    IHyprWindowDecoration(CWindow*);
    virtual ~IHyprWindowDecoration() = 0;

    virtual SWindowDecorationExtents getWindowDecorationExtents() = 0;

    virtual void                     draw(CMonitor*, float a, const Vector2D& offset = Vector2D()) = 0;

    virtual eDecorationType          getDecorationType() = 0;

    virtual void                     updateWindow(CWindow*) = 0;

    virtual void                     damageEntire() = 0;

    virtual SWindowDecorationExtents getWindowDecorationReservedArea();

    virtual CRegion                  getWindowDecorationRegion();

    virtual bool                     allowsInput();

    virtual void                     onBeginWindowDragOnDeco(const Vector2D&); // called when the user calls the "movewindow" mouse dispatcher on the deco

    virtual bool                     onEndWindowDragOnDeco(CWindow* pDraggedWindow, const Vector2D&); // returns true if the window should be placed by the layout

    virtual void                     onMouseDownOnDeco(const Vector2D&, wlr_pointer_button_event*);

  private:
    CWindow* m_pWindow = nullptr;
};
