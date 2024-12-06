#pragma once

#include <deque>
#include <string>

#include "../config/ConfigDataValues.hpp"
#include "../defines.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/TagKeeper.hpp"
#include "../macros.hpp"
#include "../managers/XWaylandManager.hpp"
#include "../render/decorations/IHyprWindowDecoration.hpp"
#include "DesktopTypes.hpp"
#include "Popup.hpp"
#include "Subsurface.hpp"
#include "WLSurface.hpp"
#include "Workspace.hpp"

class CXDGSurfaceResource;
class CXWaylandSurface;

enum eIdleInhibitMode : uint8_t {
    IDLEINHIBIT_NONE = 0,
    IDLEINHIBIT_ALWAYS,
    IDLEINHIBIT_FULLSCREEN,
    IDLEINHIBIT_FOCUS
};

enum eGroupRules : uint8_t {
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

enum eGetWindowProperties : uint8_t {
    WINDOW_ONLY      = 0,
    RESERVED_EXTENTS = 1 << 0,
    INPUT_EXTENTS    = 1 << 1,
    FULL_EXTENTS     = 1 << 2,
    FLOATING_ONLY    = 1 << 3,
    ALLOW_FLOATING   = 1 << 4,
    USE_PROP_TILED   = 1 << 5,
};

enum eSuppressEvents : uint8_t {
    SUPPRESS_NONE               = 0,
    SUPPRESS_FULLSCREEN         = 1 << 0,
    SUPPRESS_MAXIMIZE           = 1 << 1,
    SUPPRESS_ACTIVATE           = 1 << 2,
    SUPPRESS_ACTIVATE_FOCUSONLY = 1 << 3,
};

class IWindowTransformer;

struct SAlphaValue {
    float m_fAlpha;
    bool  m_bOverride;

    float applyAlpha(float alpha) const {
        if (m_bOverride)
            return m_fAlpha;
        else
            return m_fAlpha * alpha;
    };
};

enum eOverridePriority : uint8_t {
    PRIORITY_LAYOUT = 0,
    PRIORITY_WORKSPACE_RULE,
    PRIORITY_WINDOW_RULE,
    PRIORITY_SET_PROP,
};

template <typename T>
class CWindowOverridableVar {
  public:
    CWindowOverridableVar(T const& value, eOverridePriority priority) {
        values[priority] = value;
    }
    CWindowOverridableVar(T const& value) : defaultValue{value} {}

    CWindowOverridableVar()  = default;
    ~CWindowOverridableVar() = default;

    CWindowOverridableVar<T>& operator=(CWindowOverridableVar<T> const& other) {
        // Self-assignment check
        if (this == &other)
            return *this;

        for (auto const& value : other.values) {
            values[value.first] = value.second;
        }

        return *this;
    }

    void unset(eOverridePriority priority) {
        values.erase(priority);
    }

    bool hasValue() {
        return !values.empty();
    }

    T value() {
        if (!values.empty())
            return std::prev(values.end())->second;
        else
            throw std::bad_optional_access();
    }

    T valueOr(T const& other) {
        if (hasValue())
            return value();
        else
            return other;
    }

    T valueOrDefault() {
        return valueOr(defaultValue);
    }

    eOverridePriority getPriority() {
        if (!values.empty())
            return std::prev(values.end())->first;
        else
            throw std::bad_optional_access();
    }

    void matchOptional(std::optional<T> const& optValue, eOverridePriority priority) {
        if (optValue.has_value())
            values[priority] = optValue.value();
        else
            unset(priority);
    }

    operator std::optional<T>() {
        if (hasValue())
            return value();
        else
            return std::nullopt;
    }

  private:
    std::map<eOverridePriority, T> values;
    T                              defaultValue; // used for toggling, so required for bool
};

struct SWindowData {
    CWindowOverridableVar<SAlphaValue>        alpha           = SAlphaValue{1.f, false};
    CWindowOverridableVar<SAlphaValue>        alphaInactive   = SAlphaValue{1.f, false};
    CWindowOverridableVar<SAlphaValue>        alphaFullscreen = SAlphaValue{1.f, false};

    CWindowOverridableVar<bool>               allowsInput        = false;
    CWindowOverridableVar<bool>               dimAround          = false;
    CWindowOverridableVar<bool>               decorate           = true;
    CWindowOverridableVar<bool>               focusOnActivate    = false;
    CWindowOverridableVar<bool>               keepAspectRatio    = false;
    CWindowOverridableVar<bool>               nearestNeighbor    = false;
    CWindowOverridableVar<bool>               noAnim             = false;
    CWindowOverridableVar<bool>               noBorder           = false;
    CWindowOverridableVar<bool>               noBlur             = false;
    CWindowOverridableVar<bool>               noDim              = false;
    CWindowOverridableVar<bool>               noFocus            = false;
    CWindowOverridableVar<bool>               noMaxSize          = false;
    CWindowOverridableVar<bool>               noRounding         = false;
    CWindowOverridableVar<bool>               noShadow           = false;
    CWindowOverridableVar<bool>               noShortcutsInhibit = false;
    CWindowOverridableVar<bool>               opaque             = false;
    CWindowOverridableVar<bool>               RGBX               = false;
    CWindowOverridableVar<bool>               syncFullscreen     = true;
    CWindowOverridableVar<bool>               tearing            = false;
    CWindowOverridableVar<bool>               xray               = false;
    CWindowOverridableVar<bool>               renderUnfocused    = false;

    CWindowOverridableVar<int>                rounding;
    CWindowOverridableVar<int>                borderSize;

    CWindowOverridableVar<float>              scrollMouse;
    CWindowOverridableVar<float>              scrollTouchpad;

    CWindowOverridableVar<std::string>        animationStyle;
    CWindowOverridableVar<Vector2D>           maxSize;
    CWindowOverridableVar<Vector2D>           minSize;

    CWindowOverridableVar<CGradientValueData> activeBorderColor;
    CWindowOverridableVar<CGradientValueData> inactiveBorderColor;
};

struct SWindowRule {
    std::string szRule;
    std::string szValue;

    bool        v2 = false;
    std::string szTitle;
    std::string szClass;
    std::string szInitialTitle;
    std::string szInitialClass;
    std::string szTag;
    int         bX11              = -1; // -1 means "ANY"
    int         bFloating         = -1;
    int         bFullscreen       = -1;
    int         bPinned           = -1;
    int         bFocus            = -1;
    std::string szFullscreenState = ""; // empty means any
    std::string szOnWorkspace     = ""; // empty means any
    std::string szWorkspace       = ""; // empty means any
};

struct SInitialWorkspaceToken {
    PHLWINDOWREF primaryOwner;
    std::string  workspace;
};

struct SFullscreenState {
    eFullscreenMode internal = FSMODE_NONE;
    eFullscreenMode client   = FSMODE_NONE;
};

class CWindow {
  public:
    static PHLWINDOW create(SP<CXDGSurfaceResource>);
    static PHLWINDOW create(SP<CXWaylandSurface>);

  private:
    CWindow(SP<CXDGSurfaceResource> resource);
    CWindow(SP<CXWaylandSurface> surface);

  public:
    ~CWindow();

    SP<CWLSurface> m_pWLSurface;

    struct {
        CSignal destroy;
    } events;

    WP<CXDGSurfaceResource> m_pXDGSurface;
    WP<CXWaylandSurface>    m_pXWaylandSurface;

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
    bool     m_bIsPseudotiled = false;
    Vector2D m_vPseudoSize    = Vector2D(1280, 720);

    // for recovering relative cursor position
    Vector2D         m_vRelativeCursorCoordsOnLastWarp = Vector2D(-1, -1);

    bool             m_bFirstMap        = false; // for layouts
    bool             m_bIsFloating      = false;
    bool             m_bDraggingTiled   = false; // for dragging around tiled windows
    bool             m_bWasMaximized    = false;
    SFullscreenState m_sFullscreenState = {.internal = FSMODE_NONE, .client = FSMODE_NONE};
    std::string      m_szTitle          = "";
    std::string      m_szClass          = "";
    std::string      m_szInitialTitle   = "";
    std::string      m_szInitialClass   = "";
    PHLWORKSPACE     m_pWorkspace;
    PHLMONITORREF    m_pMonitor;

    bool             m_bIsMapped = false;

    bool             m_bRequestsFloat = false;

    // This is for fullscreen apps
    bool m_bCreatedOverFullscreen = false;

    // XWayland stuff
    bool         m_bIsX11 = false;
    PHLWINDOWREF m_pX11Parent;
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
    SBoxExtents              m_eOriginalClosedExtents;
    bool                     m_bAnimatingIn = false;

    // For pinned (sticky) windows
    bool m_bPinned = false;

    // For preserving pinned state when fullscreening a pinned window
    bool m_bPinFullscreened = false;

    // urgency hint
    bool m_bIsUrgent = false;

    // for proper cycling. While cycling we can't just move the pointers, so we need to keep track of the last cycled window.
    PHLWINDOWREF m_pLastCycledWindow;

    // Window decorations
    // TODO: make this a SP.
    std::deque<std::unique_ptr<IHyprWindowDecoration>> m_dWindowDecorations;
    std::vector<IHyprWindowDecoration*>                m_vDecosToRemove;

    // Special render data, rules, etc
    SWindowData m_sWindowData;

    // Transformers
    std::vector<std::unique_ptr<IWindowTransformer>> m_vTransformers;

    // for alpha
    CAnimatedVariable<float> m_fActiveInactiveAlpha;

    // animated shadow color
    CAnimatedVariable<CHyprColor> m_cRealShadowColor;

    // animated tint
    CAnimatedVariable<float> m_fDimPercent;

    // animate moving to an invisible workspace
    int                      m_iMonitorMovedFrom = -1; // -1 means not moving
    CAnimatedVariable<float> m_fMovingToWorkspaceAlpha;

    // swallowing
    PHLWINDOWREF m_pSwallowed;
    bool         m_bGroupSwallowed = false;

    // focus stuff
    bool m_bStayFocused = false;

    // for toplevel monitor events
    MONITORID m_iLastToplevelMonitorID = -1;
    MONITORID m_iLastSurfaceMonitorID  = -1;

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

    // window tags
    CTagKeeper m_tags;

    // For the list lookup
    bool operator==(const CWindow& rhs) const {
        return m_pXDGSurface == rhs.m_pXDGSurface && m_pXWaylandSurface == rhs.m_pXWaylandSurface && m_vPosition == rhs.m_vPosition && m_vSize == rhs.m_vSize &&
            m_bFadingOut == rhs.m_bFadingOut;
    }

    // methods
    CBox                   getFullWindowBoundingBox();
    SBoxExtents            getFullWindowExtents();
    CBox                   getWindowBoxUnified(uint64_t props);
    CBox                   getWindowIdealBoundingBoxIgnoreReserved();
    void                   addWindowDeco(std::unique_ptr<IHyprWindowDecoration> deco);
    void                   updateWindowDecos();
    void                   removeWindowDeco(IHyprWindowDecoration* deco);
    void                   uncacheWindowDecos();
    bool                   checkInputOnDecos(const eInputType, const Vector2D&, std::any = {});
    pid_t                  getPID();
    IHyprWindowDecoration* getDecorationByType(eDecorationType);
    void                   removeDecorationByType(eDecorationType);
    void                   updateToplevel();
    void                   updateSurfaceScaleTransformDetails(bool force = false);
    void                   moveToWorkspace(PHLWORKSPACE);
    PHLWINDOW              x11TransientFor();
    void                   onUnmap();
    void                   onMap();
    void                   setHidden(bool hidden);
    bool                   isHidden();
    void                   applyDynamicRule(const SWindowRule& r);
    void                   updateDynamicRules();
    SBoxExtents            getFullWindowReservedArea();
    Vector2D               middle();
    bool                   opaque();
    float                  rounding();
    bool                   canBeTorn();
    void                   setSuspended(bool suspend);
    bool                   visibleOnMonitor(PHLMONITOR pMonitor);
    WORKSPACEID            workspaceID();
    MONITORID              monitorID();
    bool                   onSpecialWorkspace();
    void                   activate(bool force = false);
    int                    surfacesCount();
    void                   clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize);
    bool                   isFullscreen();
    bool                   isEffectiveInternalFSMode(const eFullscreenMode);
    int                    getRealBorderSize();
    float                  getScrollMouse();
    float                  getScrollTouchpad();
    void                   updateWindowData();
    void                   updateWindowData(const struct SWorkspaceRule&);
    void                   onBorderAngleAnimEnd(void* ptr);
    bool                   isInCurvedCorner(double x, double y);
    bool                   hasPopupAt(const Vector2D& pos);
    int                    popupsCount();
    void                   applyGroupRules();
    void                   createGroup();
    void                   destroyGroup();
    PHLWINDOW              getGroupHead();
    PHLWINDOW              getGroupTail();
    PHLWINDOW              getGroupCurrent();
    PHLWINDOW              getGroupPrevious();
    PHLWINDOW              getGroupWindowByIndex(int);
    int                    getGroupSize();
    bool                   canBeGroupedInto(PHLWINDOW pWindow);
    void                   setGroupCurrent(PHLWINDOW pWindow);
    void                   insertWindowToGroup(PHLWINDOW pWindow);
    void                   updateGroupOutputs();
    void                   switchWithWindowInGroup(PHLWINDOW pWindow);
    void                   setAnimationsToMove();
    void                   onWorkspaceAnimUpdate();
    void                   onUpdateState();
    void                   onUpdateMeta();
    void                   onX11Configure(CBox box);
    void                   onResourceChangeX11();
    std::string            fetchTitle();
    std::string            fetchClass();
    void                   warpCursor(bool force = false);
    PHLWINDOW              getSwallower();
    void                   unsetWindowData(eOverridePriority priority);
    bool                   isX11OverrideRedirect();
    bool                   isModal();
    Vector2D               requestedMinSize();
    Vector2D               requestedMaxSize();

    CBox                   getWindowMainSurfaceBox() const {
        return {m_vRealPosition.value().x, m_vRealPosition.value().y, m_vRealSize.value().x, m_vRealSize.value().y};
    }

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
        CHyprSignalListener activate;
        CHyprSignalListener configure;
        CHyprSignalListener setGeometry;
        CHyprSignalListener updateState;
        CHyprSignalListener updateMetadata;
        CHyprSignalListener resourceChange;
    } listeners;

  private:
    // For hidden windows and stuff
    bool        m_bHidden        = false;
    bool        m_bSuspended     = false;
    WORKSPACEID m_iLastWorkspace = WORKSPACE_INVALID;
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
            std::format_to(out, ", monitor: {}", w->monitorID());
        if (formatClass)
            std::format_to(out, ", class: {}", w->m_szClass);
        return std::format_to(out, "]");
    }
};
