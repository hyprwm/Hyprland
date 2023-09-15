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
    bool     isInternalDecoration = false;
    bool     isReservedArea       = true; // External Decorations Only
};

void addExtentsToBox(wlr_box*, SWindowDecorationExtents*);

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

    virtual void                     forceReload();

    virtual CRegion                  getWindowDecorationRegion();

    virtual bool                     allowsInput();

    virtual void                     dragWindowToDecoration(CWindow*, const Vector2D&);

    virtual void                     clickDecoration(const Vector2D&);

    virtual void                     dragFromDecoration(const Vector2D&);

  private:
    CWindow* m_pWindow = nullptr;
};
