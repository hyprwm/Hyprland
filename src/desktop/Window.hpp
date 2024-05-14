#pragma once

#include "../defines.hpp"
#include "Subsurface.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "../render/decorations/IHyprWindowDecoration.hpp"
#include <deque>
#include "../config/ConfigDataValues.hpp"
#include "../helpers/Vector2D.hpp"
#include "WLSurface.hpp"
#include "Popup.hpp"
#include "../macros.hpp"
#include "../managers/XWaylandManager.hpp"
#include "DesktopTypes.hpp"
#include "../helpers/signal/Signal.hpp"

class CXDGSurfaceResource;

enum eIdleInhibitMode {
    IDLEINHIBIT_NONE = 0,
    IDLEINHIBIT_ALWAYS,
    IDLEINHIBIT_FULLSCREEN,
    IDLEINHIBIT_FOCUS
};

enum eGroupRules {
    // effective only during first map, except for _ALWAYS variant
    GROUP_NONE        = 0,
    GROUP_SET         = 1 << 0, // Open as new group or add to focused group
    GROUP_SET_ALWAYS  = 1 << 1,
    GROUP_BARRED      = 1 << 2, // Don't insert to focused group.
    GROUP_LOCK        = 1 << 3, // Lock m_sGroupData.lock
    GROUP_LOCK_ALWAYS = 1 << 4,
    GROUP_INVADE      = 1 << 5, // Force enter a group, event if lock is engaged
    GROUP_OVERRIDE    = 1 << 6, // Override other rules
};

enum eGetWindowProperties {
    WINDOW_ONLY      = 0,
    RESERVED_EXTENTS = 1 << 0,
    INPUT_EXTENTS    = 1 << 1,
    FULL_EXTENTS     = 1 << 2,
    FLOATING_ONLY    = 1 << 3,
    ALLOW_FLOATING   = 1 << 4,
    USE_PROP_TILED   = 1 << 5,
};

enum eSuppressEvents {
    SUPPRESS_NONE               = 0,
    SUPPRESS_FULLSCREEN         = 1 << 0,
    SUPPRESS_MAXIMIZE           = 1 << 1,
    SUPPRESS_ACTIVATE           = 1 << 2,
    SUPPRESS_ACTIVATE_FOCUSONLY = 1 << 3,
};

class IWindowTransformer;

template <typename T>
class CWindowOverridableVar {
  public:
    CWindowOverridableVar(T val) {
        value = val;
    }

    ~CWindowOverridableVar() = default;

    CWindowOverridableVar<T>& operator=(CWindowOverridableVar<T> other) {
        if (locked)
            return *this;

        locked = other.locked;
        value  = other.value;

        return *this;
    }

    T operator=(T& other) {
        if (locked)
            return value;
        value = other;
        return other;
    }

    void forceSetIgnoreLocked(T val, bool lock = false) {
        value  = val;
        locked = lock;
    }

    T operator*(T& other) {
        return value * other;
    }

    T operator+(T& other) {
        return value + other;
    }

    bool operator==(T& other) {
        return other == value;
    }

    bool operator>=(T& other) {
        return value >= other;
    }

    bool operator<=(T& other) {
        return value <= other;
    }

    bool operator>(T& other) {
        return value > other;
    }

    bool operator<(T& other) {
        return value < other;
    }

    explicit operator bool() {
        return static_cast<bool>(value);
    }

    T toUnderlying() {
        return value;
    }

    bool locked = false;

  private:
    T value;
};

struct SWindowSpecialRenderData {
    CWindowOverridableVar<bool>               alphaOverride           = false;
    CWindowOverridableVar<float>              alpha                   = 1.f;
    CWindowOverridableVar<bool>               alphaInactiveOverride   = false;
    CWindowOverridableVar<float>              alphaInactive           = -1.f; // -1 means unset
    CWindowOverridableVar<bool>               alphaFullscreenOverride = false;
    CWindowOverridableVar<float>              alphaFullscreen         = -1.f; // -1 means unset

    CWindowOverridableVar<CGradientValueData> activeBorderColor   = CGradientValueData(); // empty color vector means unset
    CWindowOverridableVar<CGradientValueData> inactiveBorderColor = CGradientValueData(); // empty color vector means unset

    // set by the layout
    CWindowOverridableVar<int> borderSize = -1; // -1 means unset
    bool                       rounding   = true;
    bool                       border     = true;
    bool                       decorate   = true;
    bool                       shadow     = true;
};

struct SWindowAdditionalConfigData {
    std::string                     animationStyle        = std::string("");
    CWindowOverridableVar<int>      rounding              = -1; // -1 means no
    CWindowOverridableVar<bool>     forceNoBlur           = false;
    CWindowOverridableVar<bool>     forceOpaque           = false;
    CWindowOverridableVar<bool>     forceOpaqueOverridden = false; // if true, a rule will not change the forceOpaque state. This is for the force opaque dispatcher.
    CWindowOverridableVar<bool>     forceAllowsInput      = false;
    CWindowOverridableVar<bool>     forceNoAnims          = false;
    CWindowOverridableVar<bool>     forceNoBorder         = false;
    CWindowOverridableVar<bool>     forceNoShadow         = false;
    CWindowOverridableVar<bool>     forceNoDim            = false;
    CWindowOverridableVar<bool>     noFocus               = false;
    CWindowOverridableVar<bool>     windowDanceCompat     = false;
    CWindowOverridableVar<bool>     noMaxSize             = false;
    CWindowOverridableVar<Vector2D> maxSize               = Vector2D(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    CWindowOverridableVar<Vector2D> minSize               = Vector2D(20, 20);
    CWindowOverridableVar<bool>     dimAround             = false;
    CWindowOverridableVar<bool>     forceRGBX             = false;
    CWindowOverridableVar<bool>     keepAspectRatio       = false;
    CWindowOverridableVar<bool>     focusOnActivate       = false;
    CWindowOverridableVar<int>      xray                  = -1; // -1 means unset, takes precedence over the renderdata one
    CWindowOverridableVar<int>      borderSize            = -1; // -1 means unset, takes precedence over the renderdata one
    CWindowOverridableVar<bool>     forceTearing          = false;
    CWindowOverridableVar<bool>     nearestNeighbor       = false;
};

struct SWindowRule {
    std::string szRule;
    std::string szValue;

    bool        v2 = false;
    std::string szTitle;
    std::string szClass;
    std::string szInitialTitle;
    std::string szInitialClass;
    int         bX11          = -1; // -1 means "ANY"
    int         bFloating     = -1;
    int         bFullscreen   = -1;
    int         bPinned       = -1;
    int         bFocus        = -1;
    std::string szOnWorkspace = ""; // empty means any
    std::string szWorkspace   = ""; // empty means any
};

struct SInitialWorkspaceToken {
    PHLWINDOWREF primaryOwner;
    std::string  workspace;
};

class CWindow {
  public:
    static PHLWINDOW create(SP<CXDGSurfaceResource>);
    // xwl
    static PHLWINDOW create();

  private:
    CWindow(SP<CXDGSurfaceResource> resource);
    CWindow();

  public:
    ~CWindow();

    DYNLISTENER(commitWindow);
    DYNLISTENER(mapWindow);
    DYNLISTENER(unmapWindow);
    DYNLISTENER(destroyWindow);
    DYNLISTENER(setTitleWindow);
    DYNLISTENER(setGeometryX11U);
    DYNLISTENER(fullscreenWindow);
    DYNLISTENER(requestMove);
    DYNLISTENER(requestMinimize);
    DYNLISTENER(requestMaximize);
    DYNLISTENER(requestResize);
    DYNLISTENER(activateX11);
    DYNLISTENER(configureX11);
    DYNLISTENER(toplevelClose);
    DYNLISTENER(toplevelActivate);
    DYNLISTENER(toplevelFullscreen);
    DYNLISTENER(setOverrideRedirect);
    DYNLISTENER(associateX11);
    DYNLISTENER(dissociateX11);
    DYNLISTENER(ackConfigure);
    // DYNLISTENER(newSubsurfaceWindow);

    CWLSurface m_pWLSurface;

    struct {
        CSignal destroy;
    } events;

    union {
        wlr_xwayland_surface* xwayland;
    } m_uSurface;
    WP<CXDGSurfaceResource> m_pXDGSurface;

    // this is the position and size of the "bounding box"
    Vector2D m_vPosition = Vector2D(0, 0);
    Vector2D m_vSize     = Vector2D(0, 0);

    // this is the real position and size used to draw the thing
    CAnimatedVariable<Vector2D> m_vRealPosition;
    CAnimatedVariable<Vector2D> m_vRealSize;

    // for not spamming the protocols
    Vector2D                                     m_vReportedPosition;
    Vector2D                                     m_vReportedSize;
    Vector2D                                     m_vPendingReportedSize;
    std::optional<std::pair<uint32_t, Vector2D>> m_pPendingSizeAck;
    std::vector<std::pair<uint32_t, Vector2D>>   m_vPendingSizeAcks;

    // for restoring floating statuses
    Vector2D m_vLastFloatingSize;
    Vector2D m_vLastFloatingPosition;

    // for floating window offset in workspace animations
    Vector2D m_vFloatingOffset = Vector2D(0, 0);

    // this is used for pseudotiling
    bool         m_bIsPseudotiled = false;
    Vector2D     m_vPseudoSize    = Vector2D(1280, 720);

    bool         m_bFirstMap           = false; // for layouts
    bool         m_bIsFloating         = false;
    bool         m_bDraggingTiled      = false; // for dragging around tiled windows
    bool         m_bIsFullscreen       = false;
    bool         m_bDontSendFullscreen = false;
    bool         m_bWasMaximized       = false;
    uint64_t     m_iMonitorID          = -1;
    std::string  m_szTitle             = "";
    std::string  m_szClass             = "";
    std::string  m_szInitialTitle      = "";
    std::string  m_szInitialClass      = "";
    PHLWORKSPACE m_pWorkspace;

    bool         m_bIsMapped = false;

    bool         m_bRequestsFloat = false;

    // This is for fullscreen apps
    bool m_bCreatedOverFullscreen = false;

    // XWayland stuff
    bool         m_bIsX11 = false;
    PHLWINDOWREF m_pX11Parent;
    uint64_t     m_iX11Type              = 0;
    bool         m_bIsModal              = false;
    bool         m_bX11DoesntWantBorders = false;
    bool         m_bX11ShouldntFocus     = false;
    float        m_fX11SurfaceScaledBy   = 1.f;
    //

    // For nofocus
    bool m_bNoInitialFocus = false;

    // Fullscreen and Maximize
    bool m_bWantsInitialFullscreen = false;

    // bitfield eSuppressEvents
    uint64_t m_eSuppressedEvents = SUPPRESS_NONE;

    // desktop components
    std::unique_ptr<CSubsurface> m_pSubsurfaceHead;
    std::unique_ptr<CPopup>      m_pPopupHead;

    // Animated border
    CGradientValueData       m_cRealBorderColor         = {0};
    CGradientValueData       m_cRealBorderColorPrevious = {0};
    CAnimatedVariable<float> m_fBorderFadeAnimationProgress;
    CAnimatedVariable<float> m_fBorderAngleAnimationProgress;

    // Fade in-out
    CAnimatedVariable<float> m_fAlpha;
    bool                     m_bFadingOut     = false;
    bool                     m_bReadyToDelete = false;
    Vector2D                 m_vOriginalClosedPos;  // these will be used for calculations later on in
    Vector2D                 m_vOriginalClosedSize; // drawing the closing animations
    SWindowDecorationExtents m_eOriginalClosedExtents;
    bool                     m_bAnimatingIn = false;

    // For pinned (sticky) windows
    bool m_bPinned = false;

    // urgency hint
    bool m_bIsUrgent = false;

    // fakefullscreen
    bool m_bFakeFullscreenState = false;

    // for proper cycling. While cycling we can't just move the pointers, so we need to keep track of the last cycled window.
    PHLWINDOWREF m_pLastCycledWindow;

    // Window decorations
    // TODO: make this a SP.
    std::deque<std::unique_ptr<IHyprWindowDecoration>> m_dWindowDecorations;
    std::vector<IHyprWindowDecoration*>                m_vDecosToRemove;

    // Special render data, rules, etc
    SWindowSpecialRenderData    m_sSpecialRenderData;
    SWindowAdditionalConfigData m_sAdditionalConfigData;

    // Transformers
    std::vector<std::unique_ptr<IWindowTransformer>> m_vTransformers;

    // for alpha
    CAnimatedVariable<float> m_fActiveInactiveAlpha;

    // animated shadow color
    CAnimatedVariable<CColor> m_cRealShadowColor;

    // animated tint
    CAnimatedVariable<float> m_fDimPercent;

    // swallowing
    PHLWINDOWREF m_pSwallowed;

    // focus stuff
    bool m_bStayFocused = false;

    // for toplevel monitor events
    uint64_t m_iLastToplevelMonitorID = -1;
    uint64_t m_iLastSurfaceMonitorID  = -1;

    // for idle inhibiting windows
    eIdleInhibitMode m_eIdleInhibitMode = IDLEINHIBIT_NONE;

    // initial token. Will be unregistered on workspace change or timeout of 2 minutes
    std::string m_szInitialWorkspaceToken = "";

    // for groups
    struct SGroupData {
        PHLWINDOWREF pNextWindow; // nullptr means no grouping. Self means single group.
        bool         head   = false;
        bool         locked = false; // per group lock
        bool         deny   = false; // deny window from enter a group or made a group
    } m_sGroupData;
    uint16_t m_eGroupRules = GROUP_NONE;

    bool     m_bTearingHint = false;

    // stores the currently matched window rules
    std::vector<SWindowRule> m_vMatchedRules;

    // For the list lookup
    bool operator==(const CWindow& rhs) {
        return m_pXDGSurface == rhs.m_pXDGSurface && m_uSurface.xwayland == rhs.m_uSurface.xwayland && m_vPosition == rhs.m_vPosition && m_vSize == rhs.m_vSize &&
            m_bFadingOut == rhs.m_bFadingOut;
    }

    // methods
    CBox                     getFullWindowBoundingBox();
    SWindowDecorationExtents getFullWindowExtents();
    CBox                     getWindowBoxUnified(uint64_t props);
    CBox                     getWindowMainSurfaceBox();
    CBox                     getWindowIdealBoundingBoxIgnoreReserved();
    void                     addWindowDeco(std::unique_ptr<IHyprWindowDecoration> deco);
    void                     updateWindowDecos();
    void                     removeWindowDeco(IHyprWindowDecoration* deco);
    void                     uncacheWindowDecos();
    bool                     checkInputOnDecos(const eInputType, const Vector2D&, std::any = {});
    pid_t                    getPID();
    IHyprWindowDecoration*   getDecorationByType(eDecorationType);
    void                     removeDecorationByType(eDecorationType);
    void                     updateToplevel();
    void                     updateSurfaceScaleTransformDetails();
    void                     moveToWorkspace(PHLWORKSPACE);
    PHLWINDOW                X11TransientFor();
    void                     onUnmap();
    void                     onMap();
    void                     setHidden(bool hidden);
    bool                     isHidden();
    void                     applyDynamicRule(const SWindowRule& r);
    void                     updateDynamicRules();
    SWindowDecorationExtents getFullWindowReservedArea();
    Vector2D                 middle();
    bool                     opaque();
    float                    rounding();
    bool                     canBeTorn();
    bool                     shouldSendFullscreenState();
    void                     setSuspended(bool suspend);
    bool                     visibleOnMonitor(CMonitor* pMonitor);
    int                      workspaceID();
    bool                     onSpecialWorkspace();
    void                     activate(bool force = false);
    int                      surfacesCount();

    int                      getRealBorderSize();
    void                     updateSpecialRenderData();
    void                     updateSpecialRenderData(const struct SWorkspaceRule&);

    void                     onBorderAngleAnimEnd(void* ptr);
    bool                     isInCurvedCorner(double x, double y);
    bool                     hasPopupAt(const Vector2D& pos);
    int                      popupsCount();

    void                     applyGroupRules();
    void                     createGroup();
    void                     destroyGroup();
    PHLWINDOW                getGroupHead();
    PHLWINDOW                getGroupTail();
    PHLWINDOW                getGroupCurrent();
    PHLWINDOW                getGroupPrevious();
    PHLWINDOW                getGroupWindowByIndex(int);
    int                      getGroupSize();
    bool                     canBeGroupedInto(PHLWINDOW pWindow);
    void                     setGroupCurrent(PHLWINDOW pWindow);
    void                     insertWindowToGroup(PHLWINDOW pWindow);
    void                     updateGroupOutputs();
    void                     switchWithWindowInGroup(PHLWINDOW pWindow);
    void                     setAnimationsToMove();
    void                     onWorkspaceAnimUpdate();
    void                     onUpdateState();
    void                     onUpdateMeta();
    std::string              fetchTitle();
    std::string              fetchClass();

    // listeners
    void onAck(uint32_t serial);

    //
    std::unordered_map<std::string, std::string> getEnv();

    //
    PHLWINDOWREF m_pSelf;

    // make private once we move listeners to inside CWindow
    struct {
        CHyprSignalListener map;
        CHyprSignalListener ack;
        CHyprSignalListener unmap;
        CHyprSignalListener commit;
        CHyprSignalListener destroy;
        CHyprSignalListener updateState;
        CHyprSignalListener updateMetadata;
    } listeners;

  private:
    // For hidden windows and stuff
    bool m_bHidden        = false;
    bool m_bSuspended     = false;
    int  m_iLastWorkspace = WORKSPACE_INVALID;
};

inline bool valid(PHLWINDOW w) {
    return w.get();
}

inline bool valid(PHLWINDOWREF w) {
    return !w.expired();
}

inline bool validMapped(PHLWINDOW w) {
    if (!valid(w))
        return false;
    return w->m_bIsMapped;
}

inline bool validMapped(PHLWINDOWREF w) {
    if (!valid(w))
        return false;
    return w->m_bIsMapped;
}

/**
    format specification
    - 'x', only address, equivalent of (uintpr_t)CWindow*
    - 'm', with monitor id
    - 'w', with workspace id
    - 'c', with application class
*/

template <typename CharT>
struct std::formatter<PHLWINDOW, CharT> : std::formatter<CharT> {
    bool formatAddressOnly = false;
    bool formatWorkspace   = false;
    bool formatMonitor     = false;
    bool formatClass       = false;
    FORMAT_PARSE(                           //
        FORMAT_FLAG('x', formatAddressOnly) //
        FORMAT_FLAG('m', formatMonitor)     //
        FORMAT_FLAG('w', formatWorkspace)   //
        FORMAT_FLAG('c', formatClass),
        PHLWINDOW)

    template <typename FormatContext>
    auto format(PHLWINDOW const& w, FormatContext& ctx) const {
        auto&& out = ctx.out();
        if (formatAddressOnly)
            return std::format_to(out, "{:x}", (uintptr_t)w.get());
        if (!w)
            return std::format_to(out, "[Window nullptr]");

        std::format_to(out, "[");
        std::format_to(out, "Window {:x}: title: \"{}\"", (uintptr_t)w.get(), w->m_szTitle);
        if (formatWorkspace)
            std::format_to(out, ", workspace: {}", w->m_pWorkspace ? w->workspaceID() : WORKSPACE_INVALID);
        if (formatMonitor)
            std::format_to(out, ", monitor: {}", w->m_iMonitorID);
        if (formatClass)
            std::format_to(out, ", class: {}", w->m_szClass);
        return std::format_to(out, "]");
    }
};
