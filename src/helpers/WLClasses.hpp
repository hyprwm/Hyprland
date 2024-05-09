#pragma once

#include "../events/Events.hpp"
#include "../defines.hpp"
#include "../desktop/Window.hpp"
#include "../desktop/Subsurface.hpp"
#include "../desktop/Popup.hpp"
#include "AnimatedVariable.hpp"
#include "../desktop/WLSurface.hpp"
#include "signal/Listener.hpp"
#include "Region.hpp"

class CMonitor;
class IPointer;
class IKeyboard;

struct SRenderData {
    CMonitor* pMonitor;
    timespec* when;
    double    x, y;

    // for iters
    void*        data    = nullptr;
    wlr_surface* surface = nullptr;
    double       w, h;

    // for rounding
    bool dontRound = true;

    // for fade
    float fadeAlpha = 1.f;

    // for alpha settings
    float alpha = 1.f;

    // for decorations (border)
    bool decorate = false;

    // for custom round values
    int rounding = -1; // -1 means not set

    // for blurring
    bool blur                  = false;
    bool blockBlurOptimization = false;

    // only for windows, not popups
    bool squishOversized = true;

    // for calculating UV
    PHLWINDOW pWindow;

    bool      popup = false;
};

struct SExtensionFindingData {
    Vector2D      origin;
    Vector2D      vec;
    wlr_surface** found;
};

struct SSeat {
    wlr_seat*     seat            = nullptr;
    wl_client*    exclusiveClient = nullptr;

    WP<IPointer>  mouse;
    WP<IKeyboard> keyboard;
};

struct SDrag {
    wlr_drag* drag = nullptr;

    DYNLISTENER(destroy);

    // Icon

    bool           iconMapped = false;

    wlr_drag_icon* dragIcon = nullptr;

    Vector2D       pos;

    DYNLISTENER(destroyIcon);
    DYNLISTENER(mapIcon);
    DYNLISTENER(unmapIcon);
    DYNLISTENER(commitIcon);
};

struct STablet {
    DYNLISTENER(Tip);
    DYNLISTENER(Axis);
    DYNLISTENER(Button);
    DYNLISTENER(Proximity);
    DYNLISTENER(Destroy);

    wlr_tablet*           wlrTablet   = nullptr;
    wlr_tablet_v2_tablet* wlrTabletV2 = nullptr;
    wlr_input_device*     wlrDevice   = nullptr;

    bool                  relativeInput = false;

    std::string           name = "";

    std::string           boundOutput = "";

    CBox                  activeArea;

    //
    bool operator==(const STablet& b) const {
        return wlrDevice == b.wlrDevice;
    }
};

struct STabletTool {
    wlr_tablet_tool*           wlrTabletTool   = nullptr;
    wlr_tablet_v2_tablet_tool* wlrTabletToolV2 = nullptr;

    wlr_tablet_v2_tablet*      wlrTabletOwnerV2 = nullptr;

    wlr_surface*               pSurface = nullptr;

    double                     tiltX = 0;
    double                     tiltY = 0;

    bool                       active = true;

    std::string                name = "";

    DYNLISTENER(TabletToolDestroy);
    DYNLISTENER(TabletToolSetCursor);

    bool operator==(const STabletTool& b) const {
        return wlrTabletTool == b.wlrTabletTool;
    }
};

struct STabletPad {
    wlr_tablet_v2_tablet_pad* wlrTabletPadV2 = nullptr;
    STablet*                  pTabletParent  = nullptr;
    wlr_input_device*         pWlrDevice     = nullptr;

    std::string               name = "";

    DYNLISTENER(Attach);
    DYNLISTENER(Button);
    DYNLISTENER(Strip);
    DYNLISTENER(Ring);
    DYNLISTENER(Destroy);

    bool operator==(const STabletPad& b) const {
        return wlrTabletPadV2 == b.wlrTabletPadV2;
    }
};

struct SSwipeGesture {
    PHLWORKSPACE pWorkspaceBegin = nullptr;

    double       delta = 0;

    int          initialDirection = 0;
    float        avgSpeed         = 0;
    int          speedPoints      = 0;
    int          touch_id         = 0;

    CMonitor*    pMonitor = nullptr;
};

struct SSwitchDevice {
    wlr_input_device* pWlrDevice = nullptr;

    int               status = -1; // uninitialized

    DYNLISTENER(destroy);
    DYNLISTENER(toggle);

    bool operator==(const SSwitchDevice& other) const {
        return pWlrDevice == other.pWlrDevice;
    }
};
