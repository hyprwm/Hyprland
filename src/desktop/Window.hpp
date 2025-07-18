#pragma once

#include <vector>
#include <string>
#include <optional>

#include "../config/ConfigDataValues.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "../helpers/TagKeeper.hpp"
#include "../macros.hpp"
#include "../managers/XWaylandManager.hpp"
#include "../render/decorations/IHyprWindowDecoration.hpp"
#include "../render/Transformer.hpp"
#include "DesktopTypes.hpp"
#include "Popup.hpp"
#include "Subsurface.hpp"
#include "WLSurface.hpp"
#include "Workspace.hpp"
#include "WindowRule.hpp"
#include "WindowOverridableVar.hpp"
#include "../protocols/types/ContentType.hpp"

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
    WINDOW_ONLY              = 0,
    RESERVED_EXTENTS         = 1 << 0,
    INPUT_EXTENTS            = 1 << 1,
    FULL_EXTENTS             = 1 << 2,
    FLOATING_ONLY            = 1 << 3,
    ALLOW_FLOATING           = 1 << 4,
    USE_PROP_TILED           = 1 << 5,
    SKIP_FULLSCREEN_PRIORITY = 1 << 6,
    FOCUS_PRIORITY           = 1 << 7,
};

enum eSuppressEvents : uint8_t {
    SUPPRESS_NONE               = 0,
    SUPPRESS_FULLSCREEN         = 1 << 0,
    SUPPRESS_MAXIMIZE           = 1 << 1,
    SUPPRESS_ACTIVATE           = 1 << 2,
    SUPPRESS_ACTIVATE_FOCUSONLY = 1 << 3,
    SUPPRESS_FULLSCREEN_OUTPUT  = 1 << 4,
};

class IWindowTransformer;

struct SAlphaValue {
    float alpha;
    bool  overridden;

    float applyAlpha(float a) const {
        if (overridden)
            return alpha;
        else
            return alpha * a;
    };
};

struct SWindowData {
    CWindowOverridableVar<SAlphaValue>        alpha           = SAlphaValue{.alpha = 1.f, .overridden = false};
    CWindowOverridableVar<SAlphaValue>        alphaInactive   = SAlphaValue{.alpha = 1.f, .overridden = false};
    CWindowOverridableVar<SAlphaValue>        alphaFullscreen = SAlphaValue{.alpha = 1.f, .overridden = false};

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
    CWindowOverridableVar<bool>               noFollowMouse      = false;
    CWindowOverridableVar<bool>               noScreenShare      = false;

    CWindowOverridableVar<Hyprlang::INT>      borderSize = {std::string("general:border_size"), Hyprlang::INT(0), std::nullopt};
    CWindowOverridableVar<Hyprlang::INT>      rounding   = {std::string("decoration:rounding"), Hyprlang::INT(0), std::nullopt};

    CWindowOverridableVar<Hyprlang::FLOAT>    roundingPower  = {std::string("decoration:rounding_power")};
    CWindowOverridableVar<Hyprlang::FLOAT>    scrollMouse    = {std::string("input:scroll_factor")};
    CWindowOverridableVar<Hyprlang::FLOAT>    scrollTouchpad = {std::string("input:touchpad:scroll_factor")};

    CWindowOverridableVar<std::string>        animationStyle;
    CWindowOverridableVar<Vector2D>           maxSize;
    CWindowOverridableVar<Vector2D>           minSize;

    CWindowOverridableVar<CGradientValueData> activeBorderColor;
    CWindowOverridableVar<CGradientValueData> inactiveBorderColor;

    CWindowOverridableVar<bool>               persistentSize;
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

    SP<CWLSurface> m_wlSurface;

    struct {
        CSignalT<> destroy;
    } m_events;

    WP<CXDGSurfaceResource> m_xdgSurface;
    WP<CXWaylandSurface>    m_xwaylandSurface;

    // this is the position and size of the "bounding box"
    Vector2D m_position = Vector2D(0, 0);
    Vector2D m_size     = Vector2D(0, 0);

    // this is the real position and size used to draw the thing
    PHLANIMVAR<Vector2D> m_realPosition;
    PHLANIMVAR<Vector2D> m_realSize;

    // for not spamming the protocols
    Vector2D                                     m_reportedPosition;
    Vector2D                                     m_reportedSize;
    Vector2D                                     m_pendingReportedSize;
    std::optional<std::pair<uint32_t, Vector2D>> m_pendingSizeAck;
    std::vector<std::pair<uint32_t, Vector2D>>   m_pendingSizeAcks;

    // for restoring floating statuses
    Vector2D m_lastFloatingSize;
    Vector2D m_lastFloatingPosition;

    // for floating window offset in workspace animations
    Vector2D m_floatingOffset = Vector2D(0, 0);

    // this is used for pseudotiling
    bool     m_isPseudotiled = false;
    Vector2D m_pseudoSize    = Vector2D(1280, 720);

    // for recovering relative cursor position
    Vector2D         m_relativeCursorCoordsOnLastWarp = Vector2D(-1, -1);

    bool             m_firstMap        = false; // for layouts
    bool             m_isFloating      = false;
    bool             m_draggingTiled   = false; // for dragging around tiled windows
    SFullscreenState m_fullscreenState = {.internal = FSMODE_NONE, .client = FSMODE_NONE};
    std::string      m_title           = "";
    std::string      m_class           = "";
    std::string      m_initialTitle    = "";
    std::string      m_initialClass    = "";
    PHLWORKSPACE     m_workspace;
    PHLMONITORREF    m_monitor;

    bool             m_isMapped = false;

    bool             m_requestsFloat = false;

    // This is for fullscreen apps
    bool m_createdOverFullscreen = false;

    // XWayland stuff
    bool  m_isX11                = false;
    bool  m_X11DoesntWantBorders = false;
    bool  m_X11ShouldntFocus     = false;
    float m_X11SurfaceScaledBy   = 1.f;
    //

    // For nofocus
    bool m_noInitialFocus = false;

    // Fullscreen and Maximize
    bool      m_wantsInitialFullscreen        = false;
    MONITORID m_wantsInitialFullscreenMonitor = MONITOR_INVALID;

    // bitfield suppressEvents
    uint64_t m_suppressedEvents = SUPPRESS_NONE;

    // desktop components
    UP<CSubsurface> m_subsurfaceHead;
    UP<CPopup>      m_popupHead;

    // Animated border
    CGradientValueData m_realBorderColor         = {0};
    CGradientValueData m_realBorderColorPrevious = {0};
    PHLANIMVAR<float>  m_borderFadeAnimationProgress;
    PHLANIMVAR<float>  m_borderAngleAnimationProgress;

    // Fade in-out
    PHLANIMVAR<float> m_alpha;
    bool              m_fadingOut     = false;
    bool              m_readyToDelete = false;
    Vector2D          m_originalClosedPos;  // these will be used for calculations later on in
    Vector2D          m_originalClosedSize; // drawing the closing animations
    SBoxExtents       m_originalClosedExtents;
    bool              m_animatingIn = false;

    // For pinned (sticky) windows
    bool m_pinned = false;

    // For preserving pinned state when fullscreening a pinned window
    bool m_pinFullscreened = false;

    // urgency hint
    bool m_isUrgent = false;

    // for proper cycling. While cycling we can't just move the pointers, so we need to keep track of the last cycled window.
    PHLWINDOWREF m_lastCycledWindow;

    // Window decorations
    // TODO: make this a SP.
    std::vector<UP<IHyprWindowDecoration>> m_windowDecorations;
    std::vector<IHyprWindowDecoration*>    m_decosToRemove;

    // Special render data, rules, etc
    SWindowData m_windowData;

    // Transformers
    std::vector<UP<IWindowTransformer>> m_transformers;

    // for alpha
    PHLANIMVAR<float> m_activeInactiveAlpha;
    PHLANIMVAR<float> m_movingFromWorkspaceAlpha;

    // animated shadow color
    PHLANIMVAR<CHyprColor> m_realShadowColor;

    // animated tint
    PHLANIMVAR<float> m_dimPercent;

    // animate moving to an invisible workspace
    int               m_monitorMovedFrom = -1; // -1 means not moving
    PHLANIMVAR<float> m_movingToWorkspaceAlpha;

    // swallowing
    PHLWINDOWREF m_swallowed;
    bool         m_currentlySwallowed = false;
    bool         m_groupSwallowed     = false;

    // focus stuff
    bool m_stayFocused = false;

    // for toplevel monitor events
    MONITORID m_lastSurfaceMonitorID = -1;

    // for idle inhibiting windows
    eIdleInhibitMode m_idleInhibitMode = IDLEINHIBIT_NONE;

    // initial token. Will be unregistered on workspace change or timeout of 2 minutes
    std::string m_initialWorkspaceToken = "";

    // for groups
    struct SGroupData {
        PHLWINDOWREF pNextWindow; // nullptr means no grouping. Self means single group.
        bool         head   = false;
        bool         locked = false; // per group lock
        bool         deny   = false; // deny window from enter a group or made a group
    } m_groupData;
    uint16_t m_groupRules = GROUP_NONE;

    bool     m_tearingHint = false;

    // stores the currently matched window rules
    std::vector<SP<CWindowRule>> m_matchedRules;

    // window tags
    CTagKeeper m_tags;

    // ANR
    PHLANIMVAR<float> m_notRespondingTint;

    // For the noclosefor windowrule
    Time::steady_tp m_closeableSince = Time::steadyNow();

    // For the list lookup
    bool operator==(const CWindow& rhs) const {
        return m_xdgSurface == rhs.m_xdgSurface && m_xwaylandSurface == rhs.m_xwaylandSurface && m_position == rhs.m_position && m_size == rhs.m_size &&
            m_fadingOut == rhs.m_fadingOut;
    }

    // methods
    CBox                       getFullWindowBoundingBox();
    SBoxExtents                getFullWindowExtents();
    CBox                       getWindowBoxUnified(uint64_t props);
    SBoxExtents                getWindowExtentsUnified(uint64_t props);
    CBox                       getWindowIdealBoundingBoxIgnoreReserved();
    void                       addWindowDeco(UP<IHyprWindowDecoration> deco);
    void                       updateWindowDecos();
    void                       removeWindowDeco(IHyprWindowDecoration* deco);
    void                       uncacheWindowDecos();
    bool                       checkInputOnDecos(const eInputType, const Vector2D&, std::any = {});
    pid_t                      getPID();
    IHyprWindowDecoration*     getDecorationByType(eDecorationType);
    void                       updateToplevel();
    void                       updateSurfaceScaleTransformDetails(bool force = false);
    void                       moveToWorkspace(PHLWORKSPACE);
    PHLWINDOW                  x11TransientFor();
    void                       onUnmap();
    void                       onMap();
    void                       setHidden(bool hidden);
    bool                       isHidden();
    void                       applyDynamicRule(const SP<CWindowRule>& r);
    void                       updateDynamicRules();
    SBoxExtents                getFullWindowReservedArea();
    Vector2D                   middle();
    bool                       opaque();
    float                      rounding();
    float                      roundingPower();
    bool                       canBeTorn();
    void                       setSuspended(bool suspend);
    bool                       visibleOnMonitor(PHLMONITOR pMonitor);
    WORKSPACEID                workspaceID();
    MONITORID                  monitorID();
    bool                       onSpecialWorkspace();
    void                       activate(bool force = false);
    int                        surfacesCount();
    void                       clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize);
    bool                       isFullscreen();
    bool                       isEffectiveInternalFSMode(const eFullscreenMode);
    int                        getRealBorderSize();
    float                      getScrollMouse();
    float                      getScrollTouchpad();
    void                       updateWindowData();
    void                       updateWindowData(const struct SWorkspaceRule&);
    void                       onBorderAngleAnimEnd(WP<Hyprutils::Animation::CBaseAnimatedVariable> pav);
    bool                       isInCurvedCorner(double x, double y);
    bool                       hasPopupAt(const Vector2D& pos);
    int                        popupsCount();
    void                       applyGroupRules();
    void                       createGroup();
    void                       destroyGroup();
    PHLWINDOW                  getGroupHead();
    PHLWINDOW                  getGroupTail();
    PHLWINDOW                  getGroupCurrent();
    PHLWINDOW                  getGroupPrevious();
    PHLWINDOW                  getGroupWindowByIndex(int);
    int                        getGroupSize();
    bool                       canBeGroupedInto(PHLWINDOW pWindow);
    void                       setGroupCurrent(PHLWINDOW pWindow);
    void                       insertWindowToGroup(PHLWINDOW pWindow);
    void                       updateGroupOutputs();
    void                       switchWithWindowInGroup(PHLWINDOW pWindow);
    void                       setAnimationsToMove();
    void                       onWorkspaceAnimUpdate();
    void                       onFocusAnimUpdate();
    void                       onUpdateState();
    void                       onUpdateMeta();
    void                       onX11ConfigureRequest(CBox box);
    void                       onResourceChangeX11();
    std::string                fetchTitle();
    std::string                fetchClass();
    void                       warpCursor(bool force = false);
    PHLWINDOW                  getSwallower();
    void                       unsetWindowData(eOverridePriority priority);
    bool                       isX11OverrideRedirect();
    bool                       isModal();
    Vector2D                   requestedMinSize();
    Vector2D                   requestedMaxSize();
    Vector2D                   realToReportSize();
    Vector2D                   realToReportPosition();
    Vector2D                   xwaylandSizeToReal(Vector2D size);
    Vector2D                   xwaylandPositionToReal(Vector2D size);
    void                       updateX11SurfaceScale();
    void                       sendWindowSize(bool force = false);
    NContentType::eContentType getContentType();
    void                       setContentType(NContentType::eContentType contentType);
    void                       deactivateGroupMembers();
    bool                       isNotResponding();
    std::optional<std::string> xdgTag();
    std::optional<std::string> xdgDescription();
    PHLWINDOW                  parent();
    bool                       priorityFocus();

    CBox                       getWindowMainSurfaceBox() const {
        return {m_realPosition->value().x, m_realPosition->value().y, m_realSize->value().x, m_realSize->value().y};
    }

    // listeners
    void onAck(uint32_t serial);

    //
    std::unordered_map<std::string, std::string> getEnv();

    //
    PHLWINDOWREF m_self;

    // make private once we move listeners to inside CWindow
    struct {
        CHyprSignalListener map;
        CHyprSignalListener ack;
        CHyprSignalListener unmap;
        CHyprSignalListener commit;
        CHyprSignalListener destroy;
        CHyprSignalListener activate;
        CHyprSignalListener configureRequest;
        CHyprSignalListener setGeometry;
        CHyprSignalListener updateState;
        CHyprSignalListener updateMetadata;
        CHyprSignalListener resourceChange;
    } m_listeners;

  private:
    // For hidden windows and stuff
    bool        m_hidden        = false;
    bool        m_suspended     = false;
    WORKSPACEID m_lastWorkspace = WORKSPACE_INVALID;
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
    return w->m_isMapped;
}

inline bool validMapped(PHLWINDOWREF w) {
    if (!valid(w))
        return false;
    return w->m_isMapped;
}

namespace NWindowProperties {
    static const std::unordered_map<std::string, std::function<CWindowOverridableVar<bool>*(const PHLWINDOW&)>> boolWindowProperties = {
        {"allowsinput", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.allowsInput; }},
        {"dimaround", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.dimAround; }},
        {"decorate", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.decorate; }},
        {"focusonactivate", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.focusOnActivate; }},
        {"keepaspectratio", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.keepAspectRatio; }},
        {"nearestneighbor", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.nearestNeighbor; }},
        {"noanim", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noAnim; }},
        {"noblur", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noBlur; }},
        {"noborder", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noBorder; }},
        {"nodim", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noDim; }},
        {"nofocus", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noFocus; }},
        {"nomaxsize", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noMaxSize; }},
        {"norounding", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noRounding; }},
        {"noshadow", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noShadow; }},
        {"noshortcutsinhibit", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noShortcutsInhibit; }},
        {"opaque", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.opaque; }},
        {"forcergbx", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.RGBX; }},
        {"syncfullscreen", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.syncFullscreen; }},
        {"immediate", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.tearing; }},
        {"xray", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.xray; }},
        {"nofollowmouse", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noFollowMouse; }},
        {"noscreenshare", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.noScreenShare; }},
    };

    const std::unordered_map<std::string, std::function<CWindowOverridableVar<Hyprlang::INT>*(const PHLWINDOW&)>> intWindowProperties = {
        {"rounding", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.rounding; }},
        {"bordersize", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.borderSize; }},
    };

    const std::unordered_map<std::string, std::function<CWindowOverridableVar<Hyprlang::FLOAT>*(PHLWINDOW)>> floatWindowProperties = {
        {"roundingpower", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.roundingPower; }},
        {"scrollmouse", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.scrollMouse; }},
        {"scrolltouchpad", [](const PHLWINDOW& pWindow) { return &pWindow->m_windowData.scrollTouchpad; }},
    };
};

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
        std::format_to(out, "Window {:x}: title: \"{}\"", (uintptr_t)w.get(), w->m_title);
        if (formatWorkspace)
            std::format_to(out, ", workspace: {}", w->m_workspace ? w->workspaceID() : WORKSPACE_INVALID);
        if (formatMonitor)
            std::format_to(out, ", monitor: {}", w->monitorID());
        if (formatClass)
            std::format_to(out, ", class: {}", w->m_class);
        return std::format_to(out, "]");
    }
};
