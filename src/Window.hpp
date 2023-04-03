#pragma once

#include "defines.hpp"
#include "events/Events.hpp"
#include "helpers/SubsurfaceTree.hpp"
#include "helpers/AnimatedVariable.hpp"
#include "render/decorations/IHyprWindowDecoration.hpp"
#include <deque>
#include "config/ConfigDataValues.hpp"
#include "helpers/Vector2D.hpp"
#include "helpers/WLSurface.hpp"

enum eIdleInhibitMode
{
    IDLEINHIBIT_NONE = 0,
    IDLEINHIBIT_ALWAYS,
    IDLEINHIBIT_FULLSCREEN,
    IDLEINHIBIT_FOCUS
};

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
    CWindowOverridableVar<bool>    alphaOverride         = false;
    CWindowOverridableVar<float>   alpha                 = 1.f;
    CWindowOverridableVar<bool>    alphaInactiveOverride = false;
    CWindowOverridableVar<float>   alphaInactive         = -1.f; // -1 means unset

    CWindowOverridableVar<int64_t> activeBorderColor   = -1; // -1 means unset
    CWindowOverridableVar<int64_t> inactiveBorderColor = -1; // -1 means unset

    // set by the layout
    bool rounding = true;
    bool border   = true;
    bool decorate = true;
};

struct SWindowAdditionalConfigData {
    std::string                 animationStyle        = std::string("");
    CWindowOverridableVar<int>  rounding              = -1; // -1 means no
    CWindowOverridableVar<bool> forceNoBlur           = false;
    CWindowOverridableVar<bool> forceOpaque           = false;
    CWindowOverridableVar<bool> forceOpaqueOverridden = false; // if true, a rule will not change the forceOpaque state. This is for the force opaque dispatcher.
    CWindowOverridableVar<bool> forceAllowsInput      = false;
    CWindowOverridableVar<bool> forceNoAnims          = false;
    CWindowOverridableVar<bool> forceNoBorder         = false;
    CWindowOverridableVar<bool> forceNoShadow         = false;
    CWindowOverridableVar<bool> windowDanceCompat     = false;
    CWindowOverridableVar<bool> noMaxSize             = false;
    CWindowOverridableVar<bool> dimAround             = false;
    CWindowOverridableVar<bool> forceRGBX             = false;
};

struct SWindowRule {
    std::string szRule;
    std::string szValue;

    bool        v2 = false;
    std::string szTitle;
    std::string szClass;
    int         bX11        = -1; // -1 means "ANY"
    int         bFloating   = -1;
    int         bFullscreen = -1;
    int         bPinned     = -1;
};

class CWindow {
  public:
    CWindow();
    ~CWindow();

    DYNLISTENER(commitWindow);
    DYNLISTENER(mapWindow);
    DYNLISTENER(unmapWindow);
    DYNLISTENER(destroyWindow);
    DYNLISTENER(setTitleWindow);
    DYNLISTENER(setGeometryX11U);
    DYNLISTENER(fullscreenWindow);
    DYNLISTENER(newPopupXDG);
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
    // DYNLISTENER(newSubsurfaceWindow);

    CWLSurface            m_pWLSurface;
    std::list<CWLSurface> m_lPopupSurfaces;

    union {
        wlr_xdg_surface*      xdg;
        wlr_xwayland_surface* xwayland;
    } m_uSurface;

    // this is the position and size of the "bounding box"
    Vector2D m_vPosition = Vector2D(0, 0);
    Vector2D m_vSize     = Vector2D(0, 0);

    // this is the real position and size used to draw the thing
    CAnimatedVariable m_vRealPosition;
    CAnimatedVariable m_vRealSize;

    // for not spamming the protocols
    Vector2D m_vReportedPosition;
    Vector2D m_vReportedSize;

    // for restoring floating statuses
    Vector2D m_vLastFloatingSize;
    Vector2D m_vLastFloatingPosition;

    // this is used for pseudotiling
    bool        m_bIsPseudotiled = false;
    Vector2D    m_vPseudoSize    = Vector2D(0, 0);

    uint64_t    m_iTags          = 0;
    bool        m_bIsFloating    = false;
    bool        m_bDraggingTiled = false; // for dragging around tiled windows
    bool        m_bIsFullscreen  = false;
    bool        m_bWasMaximized  = false;
    uint64_t    m_iMonitorID     = -1;
    std::string m_szTitle        = "";
    std::string m_szInitialTitle = "";
    std::string m_szInitialClass = "";
    int         m_iWorkspaceID   = -1;

    bool        m_bIsMapped = false;

    bool        m_bRequestsFloat = false;

    // This is for fullscreen apps
    bool m_bCreatedOverFullscreen = false;

    // XWayland stuff
    bool     m_bIsX11                = false;
    bool     m_bMappedX11            = false;
    CWindow* m_pX11Parent            = nullptr;
    uint64_t m_iX11Type              = 0;
    bool     m_bIsModal              = false;
    bool     m_bX11DoesntWantBorders = false;
    bool     m_bX11ShouldntFocus     = false;
    //

    // For nofocus
    bool m_bNoFocus        = false;
    bool m_bNoInitialFocus = false;

    // initial fullscreen and fullscreen disabled
    bool              m_bWantsInitialFullscreen = false;
    bool              m_bNoFullscreenRequest    = false;

    SSurfaceTreeNode* m_pSurfaceTree = nullptr;

    // Animated border
    CGradientValueData m_cRealBorderColor         = {0};
    CGradientValueData m_cRealBorderColorPrevious = {0};
    CAnimatedVariable  m_fBorderFadeAnimationProgress;
    CAnimatedVariable  m_fBorderAngleAnimationProgress;

    // Fade in-out
    CAnimatedVariable m_fAlpha;
    bool              m_bFadingOut     = false;
    bool              m_bReadyToDelete = false;
    Vector2D          m_vOriginalClosedPos;  // these will be used for calculations later on in
    Vector2D          m_vOriginalClosedSize; // drawing the closing animations

    // For pinned (sticky) windows
    bool m_bPinned = false;

    // urgency hint
    bool m_bIsUrgent = false;

    // fakefullscreen
    bool m_bFakeFullscreenState = false;

    // for proper cycling. While cycling we can't just move the pointers, so we need to keep track of the last cycled window.
    CWindow* m_pLastCycledWindow = nullptr;

    // Foreign Toplevel proto
    wlr_foreign_toplevel_handle_v1* m_phForeignToplevel = nullptr;

    // Window decorations
    std::deque<std::unique_ptr<IHyprWindowDecoration>> m_dWindowDecorations;
    std::vector<IHyprWindowDecoration*>                m_vDecosToRemove;

    // Special render data, rules, etc
    SWindowSpecialRenderData    m_sSpecialRenderData;
    SWindowAdditionalConfigData m_sAdditionalConfigData;

    // for alpha
    CAnimatedVariable m_fActiveInactiveAlpha;

    // animated shadow color
    CAnimatedVariable m_cRealShadowColor;

    // animated tint
    CAnimatedVariable m_fDimPercent;

    // swallowing
    CWindow* m_pSwallowed = nullptr;

    // for toplevel monitor events
    uint64_t m_iLastToplevelMonitorID = -1;
    uint64_t m_iLastSurfaceMonitorID  = -1;

    // for idle inhibiting windows
    eIdleInhibitMode m_eIdleInhibitMode = IDLEINHIBIT_NONE;

    // for groups
    struct SGroupData {
        CWindow* pNextWindow = nullptr; // nullptr means no grouping. Self means single group.
        bool     head        = false;
    } m_sGroupData;

    // For the list lookup
    bool operator==(const CWindow& rhs) {
        return m_uSurface.xdg == rhs.m_uSurface.xdg && m_uSurface.xwayland == rhs.m_uSurface.xwayland && m_vPosition == rhs.m_vPosition && m_vSize == rhs.m_vSize &&
            m_bFadingOut == rhs.m_bFadingOut;
    }

    // methods
    wlr_box                  getFullWindowBoundingBox();
    wlr_box                  getWindowInputBox();
    wlr_box                  getWindowIdealBoundingBoxIgnoreReserved();
    void                     updateWindowDecos();
    pid_t                    getPID();
    IHyprWindowDecoration*   getDecorationByType(eDecorationType);
    void                     removeDecorationByType(eDecorationType);
    void                     createToplevelHandle();
    void                     destroyToplevelHandle();
    void                     updateToplevel();
    void                     updateSurfaceOutputs();
    void                     moveToWorkspace(int);
    CWindow*                 X11TransientFor();
    void                     onUnmap();
    void                     onMap();
    void                     setHidden(bool hidden);
    bool                     isHidden();
    void                     applyDynamicRule(const SWindowRule& r);
    void                     updateDynamicRules();
    SWindowDecorationExtents getFullWindowReservedArea();

    void                     onBorderAngleAnimEnd(void* ptr);
    bool                     isInCurvedCorner(double x, double y);
    bool                     hasPopupAt(const Vector2D& pos);

    CWindow*                 getGroupHead();
    CWindow*                 getGroupTail();
    CWindow*                 getGroupCurrent();
    void                     setGroupCurrent(CWindow* pWindow);
    void                     insertWindowToGroup(CWindow* pWindow);
    void                     updateGroupOutputs();

  private:
    // For hidden windows and stuff
    bool m_bHidden = false;
};
