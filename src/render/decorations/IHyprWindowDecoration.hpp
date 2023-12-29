#pragma once

#include <any>
#include "../../defines.hpp"
#include "../../helpers/Region.hpp"
#include "DecorationPositioner.hpp"

enum eDecorationType {
    DECORATION_NONE = -1,
    DECORATION_GROUPBAR,
    DECORATION_SHADOW,
    DECORATION_BORDER,
    DECORATION_CUSTOM
};

enum eDecorationLayer {
    DECORATION_LAYER_BOTTOM = 0, /* lowest. */
    DECORATION_LAYER_UNDER,      /* under the window, but above BOTTOM */
    DECORATION_LAYER_OVER,       /* above the window, but below its popups */
    DECORATION_LAYER_OVERLAY     /* above everything of the window, including popups */
};

enum eDecorationFlags {
    DECORATION_ALLOWS_MOUSE_INPUT  = 1 << 0, /* this decoration accepts mouse input */
    DECORATION_PART_OF_MAIN_WINDOW = 1 << 1, /* this decoration is a *seamless* part of the main window, so stuff like shadows will include it */
    DECORATION_NON_SOLID           = 1 << 2, /* this decoration is not solid. Other decorations should draw on top of it. Example: shadow */
};

class CWindow;
class CMonitor;
class CDecorationPositioner;

class IHyprWindowDecoration {
  public:
    IHyprWindowDecoration(CWindow*);
    virtual ~IHyprWindowDecoration() = 0;

    virtual SDecorationPositioningInfo getPositioningInfo() = 0;

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply) = 0;

    virtual void                       draw(CMonitor*, float a, const Vector2D& offset = Vector2D()) = 0;

    virtual eDecorationType            getDecorationType() = 0;

    virtual void                       updateWindow(CWindow*) = 0;

    virtual void                       damageEntire() = 0; // should be ignored by non-absolute decos

    virtual bool                       onInputOnDeco(const eInputType, const Vector2D&, std::any = {});

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    virtual std::string                getDisplayName();

  private:
    CWindow* m_pWindow = nullptr;

    friend class CDecorationPositioner;
};
