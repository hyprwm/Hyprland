#pragma once

#include "../events/Events.hpp"
#include "../defines.hpp"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "../Window.hpp"
#include "../desktop/Subsurface.hpp"
#include "../desktop/Popup.hpp"
#include "AnimatedVariable.hpp"
#include "../desktop/WLSurface.hpp"
#include "Region.hpp"

struct SLayerRule {
    std::string targetNamespace = "";
    std::string rule            = "";
};

struct SLayerSurface {
    SLayerSurface();
    ~SLayerSurface();

    void                        applyRules();
    void                        startAnimation(bool in, bool instant = false);
    bool                        isFadedOut();

    CAnimatedVariable<Vector2D> realPosition;
    CAnimatedVariable<Vector2D> realSize;

    wlr_layer_surface_v1*       layerSurface;
    wl_list                     link;

    bool                        keyboardExclusive = false;

    CWLSurface                  surface;

    // desktop components
    std::unique_ptr<CPopup> popupHead;

    DYNLISTENER(destroyLayerSurface);
    DYNLISTENER(mapLayerSurface);
    DYNLISTENER(unmapLayerSurface);
    DYNLISTENER(commitLayerSurface);

    CBox                       geometry = {0, 0, 0, 0};
    Vector2D                   position;
    zwlr_layer_shell_v1_layer  layer;

    bool                       mapped = false;

    int                        monitorID = -1;

    std::string                szNamespace = "";

    CAnimatedVariable<float>   alpha;
    bool                       fadingOut     = false;
    bool                       readyToDelete = false;
    bool                       noProcess     = false;
    bool                       noAnimations  = false;

    bool                       forceBlur        = false;
    int                        xray             = -1;
    bool                       ignoreAlpha      = false;
    float                      ignoreAlphaValue = 0.f;

    std::optional<std::string> animationStyle;

    // For the list lookup
    bool operator==(const SLayerSurface& rhs) const {
        return layerSurface == rhs.layerSurface && monitorID == rhs.monitorID;
    }
};

class CMonitor;

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
    CWindow* pWindow = nullptr;

    bool     popup = false;
};

struct SExtensionFindingData {
    Vector2D      origin;
    Vector2D      vec;
    wlr_surface** found;
};

struct SStringRuleNames {
    std::string layout  = "";
    std::string model   = "";
    std::string variant = "";
    std::string options = "";
    std::string rules   = "";
};

struct SKeyboard {
    wlr_input_device* keyboard;

    DYNLISTENER(keyboardMod);
    DYNLISTENER(keyboardKey);
    DYNLISTENER(keyboardKeymap);
    DYNLISTENER(keyboardDestroy);

    bool               isVirtual = false;
    bool               active    = false;
    bool               enabled   = true;

    xkb_layout_index_t activeLayout        = 0;
    xkb_state*         xkbTranslationState = nullptr;

    std::string        name        = "";
    std::string        xkbFilePath = "";

    SStringRuleNames   currentRules;
    int                repeatRate        = 0;
    int                repeatDelay       = 0;
    int                numlockOn         = -1;
    bool               resolveBindsBySym = false;

    void               updateXKBTranslationState(xkb_keymap* const keymap = nullptr);

    // For the list lookup
    bool operator==(const SKeyboard& rhs) const {
        return keyboard == rhs.keyboard;
    }
};

struct SMouse {
    wlr_input_device* mouse = nullptr;

    std::string       name = "";

    bool              virt = false;

    bool              connected = false; // means connected to the cursor

    DYNLISTENER(destroyMouse);

    bool operator==(const SMouse& b) const {
        return mouse == b.mouse;
    }
};

class CMonitor;

struct SSeat {
    wlr_seat*  seat            = nullptr;
    wl_client* exclusiveClient = nullptr;

    SMouse*    mouse = nullptr;
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

struct SIdleInhibitor {
    wlr_idle_inhibitor_v1* pWlrInhibitor = nullptr;
    CWindow*               pWindow       = nullptr;

    DYNLISTENER(Destroy);

    bool operator==(const SIdleInhibitor& b) const {
        return pWlrInhibitor == b.pWlrInhibitor;
    }
};

struct SSwipeGesture {
    CWorkspace* pWorkspaceBegin = nullptr;

    double      delta = 0;

    int         initialDirection = 0;
    float       avgSpeed         = 0;
    int         speedPoints      = 0;
    int         touch_id         = 0;

    CMonitor*   pMonitor = nullptr;
};

struct STextInputV1;

struct STextInput {
    wlr_text_input_v3* pWlrInput = nullptr;
    STextInputV1*      pV1Input  = nullptr;

    wlr_surface*       pPendingSurface = nullptr;

    DYNLISTENER(textInputEnable);
    DYNLISTENER(textInputDisable);
    DYNLISTENER(textInputCommit);
    DYNLISTENER(textInputDestroy);

    DYNLISTENER(pendingSurfaceDestroy);
};

struct SIMEKbGrab {
    wlr_input_method_keyboard_grab_v2* pWlrKbGrab = nullptr;

    wlr_keyboard*                      pKeyboard = nullptr;

    DYNLISTENER(grabDestroy);
};

struct SIMEPopup {
    wlr_input_popup_surface_v2* pSurface = nullptr;

    int                         x, y;
    int                         realX, realY;
    bool                        visible;
    Vector2D                    lastSize;

    DYNLISTENER(mapPopup);
    DYNLISTENER(unmapPopup);
    DYNLISTENER(destroyPopup);
    DYNLISTENER(commitPopup);

    DYNLISTENER(focusedSurfaceUnmap);

    bool operator==(const SIMEPopup& other) const {
        return pSurface == other.pSurface;
    }
};

struct STouchDevice {
    wlr_input_device* pWlrDevice = nullptr;

    std::string       name = "";

    std::string       boundOutput = "";

    DYNLISTENER(destroy);

    bool operator==(const STouchDevice& other) const {
        return pWlrDevice == other.pWlrDevice;
    }
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

struct STearingController {
    wlr_tearing_control_v1* pWlrHint = nullptr;

    DYNLISTENER(set);
    DYNLISTENER(destroy);

    bool operator==(const STearingController& other) const {
        return pWlrHint == other.pWlrHint;
    }
};

struct SShortcutInhibitor {
    wlr_keyboard_shortcuts_inhibitor_v1* pWlrInhibitor = nullptr;

    DYNLISTENER(destroy);

    bool operator==(const SShortcutInhibitor& other) const {
        return pWlrInhibitor == other.pWlrInhibitor;
    }
};
