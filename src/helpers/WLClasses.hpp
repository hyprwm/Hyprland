#pragma once

#include "../events/Events.hpp"
#include "../defines.hpp"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "../Window.hpp"
#include "SubsurfaceTree.hpp"
#include "AnimatedVariable.hpp"

struct SLayerSurface {
    SLayerSurface();

    wlr_layer_surface_v1*   layerSurface;
    wl_list                 link;

    DYNLISTENER(destroyLayerSurface);
    DYNLISTENER(mapLayerSurface);
    DYNLISTENER(unmapLayerSurface);
    DYNLISTENER(commitLayerSurface);
    DYNLISTENER(newPopup);

    wlr_box                 geometry = {0,0,0,0};
    Vector2D                position;
    zwlr_layer_shell_v1_layer layer;

    int                     monitorID = -1;

    std::string             szNamespace = "";

    CAnimatedVariable       alpha;
    bool                    fadingOut = false;
    bool                    readyToDelete = false;
    bool                    noProcess = false;

    bool                    forceBlur = false;

    // For the list lookup
    bool operator==(const SLayerSurface& rhs) {
        return layerSurface == rhs.layerSurface && monitorID == rhs.monitorID;
    }
};

struct SRenderData {
    wlr_output* output;
    timespec* when;
    int x, y;

    // for iters
    void* data = nullptr;
    wlr_surface* surface = nullptr;
    int w, h;
    void* pMonitor = nullptr;

    // for rounding
    bool dontRound = true;

    // for fade
    float fadeAlpha = 255.f;

    // for alpha settings
    float alpha = 1.f;

    // for decorations (border)
    bool decorate = false;

    // for custom round values
    int rounding = -1; // -1 means not set

    // for blurring
    bool blur = false;

    // only for windows, not popups
    bool squishOversized = true;
};

struct SExtensionFindingData {
    Vector2D origin;
    Vector2D vec;
    wlr_surface** found;
};

struct SStringRuleNames {
    std::string layout = "";
    std::string model = "";
    std::string variant = "";
    std::string options = "";
    std::string rules = "";
};

struct SKeyboard {
    wlr_input_device* keyboard;

    DYNLISTENER(keyboardMod);
    DYNLISTENER(keyboardKey);
    DYNLISTENER(keyboardDestroy);

    bool active = false;

    std::string name = "";

    SStringRuleNames currentRules;
    int repeatRate = 0;
    int repeatDelay = 0;
    int numlockOn = -1;

    // For the list lookup
    bool operator==(const SKeyboard& rhs) {
        return keyboard == rhs.keyboard;
    }
};

struct SMouse {
    wlr_input_device* mouse = nullptr;

    wlr_pointer_constraint_v1* currentConstraint = nullptr;
    bool                       constraintActive = false;

    pixman_region32_t confinedTo;

    std::string name = "";

    bool        virt = false;

    DYNLISTENER(commitConstraint);
    DYNLISTENER(destroyMouse);

    bool operator==(const SMouse& b) {
        return mouse == b.mouse;
    }
};

struct SConstraint {
    SMouse* pMouse = nullptr;
    wlr_pointer_constraint_v1* constraint = nullptr;

    DYNLISTENER(setConstraintRegion);
    DYNLISTENER(destroyConstraint);

    bool operator==(const SConstraint& b) {
        return constraint == b.constraint;
    }
};

struct SMonitor;

struct SXDGPopup {
    CWindow*        parentWindow = nullptr;
    SXDGPopup*      parentPopup = nullptr;
    wlr_xdg_popup*  popup = nullptr;
    SMonitor*       monitor = nullptr;

    DYNLISTENER(newPopupFromPopupXDG);
    DYNLISTENER(destroyPopupXDG);
    DYNLISTENER(mapPopupXDG);
    DYNLISTENER(unmapPopupXDG);

    double lx;
    double ly;

    SSurfaceTreeNode* pSurfaceTree = nullptr;

    // For the list lookup
    bool operator==(const SXDGPopup& rhs) {
        return popup == rhs.popup;
    }
};

struct SSeat {
    wlr_seat*       seat = nullptr;
    wl_client*      exclusiveClient = nullptr;

    SMouse*         mouse = nullptr;
};

struct SDrag {
    wlr_drag*       drag = nullptr;

    DYNLISTENER(destroy);

    // Icon

    bool            iconMapped = false;

    wlr_drag_icon*  dragIcon = nullptr;
    
    Vector2D        pos;

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

    wlr_tablet* wlrTablet = nullptr;
    wlr_tablet_v2_tablet* wlrTabletV2 = nullptr;
    wlr_input_device* wlrDevice = nullptr;

    std::string name = "";

    bool operator==(const STablet& b) {
        return wlrDevice == b.wlrDevice;
    }
};

struct STabletTool {
    wlr_tablet_tool* wlrTabletTool = nullptr;
    wlr_tablet_v2_tablet_tool* wlrTabletToolV2 = nullptr;

    wlr_tablet_v2_tablet* wlrTabletOwnerV2 = nullptr;

    wlr_surface* pSurface = nullptr;

    double tiltX = 0;
    double tiltY = 0;

    bool active = true;

    std::string name = "";

    DYNLISTENER(TabletToolDestroy);
    DYNLISTENER(TabletToolSetCursor);

    bool operator==(const STabletTool& b) {
        return wlrTabletTool == b.wlrTabletTool;
    }
};

struct STabletPad {
    wlr_tablet_v2_tablet_pad* wlrTabletPadV2 = nullptr;
    STablet* pTabletParent = nullptr;

    std::string name = "";

    DYNLISTENER(Attach);
    DYNLISTENER(Button);
    DYNLISTENER(Strip);
    DYNLISTENER(Ring);
    DYNLISTENER(Destroy);

    bool operator==(const STabletPad& b) {
        return wlrTabletPadV2 == b.wlrTabletPadV2;
    }
};

struct SIdleInhibitor {
    wlr_idle_inhibitor_v1* pWlrInhibitor = nullptr;
    CWindow* pWindow = nullptr;

    DYNLISTENER(Destroy);

    bool operator==(const SIdleInhibitor& b) {
        return pWlrInhibitor == b.pWlrInhibitor;
    }
};

struct SSwipeGesture {
    CWorkspace*  pWorkspaceBegin = nullptr;

    double       delta = 0;

    float        avgSpeed = 0;
    int          speedPoints = 0;

    SMonitor*    pMonitor = nullptr;
};