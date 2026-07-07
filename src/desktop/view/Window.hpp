#pragma once

#include <vector>
#include <string>
#include <optional>

#include "View.hpp"
#include "../../config/shared/complex/ComplexDataTypes.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../helpers/TagKeeper.hpp"
#include "../../macros.hpp"
#include "../../managers/XWaylandManager.hpp"
#include "../../render/decorations/IHyprWindowDecoration.hpp"
#include "../../render/transformer/Transformer.hpp"
#include "../DesktopTypes.hpp"
#include "../types/MultiAnimatedVariable.hpp"
#include "Popup.hpp"
#include "Subsurface.hpp"
#include "WLSurface.hpp"
#include "../Workspace.hpp"
#include "../rule/windowRule/WindowRuleApplicator.hpp"
#include "../../protocols/types/ContentType.hpp"
#include "../../render/Framebuffer.hpp"
#include "types/GeometricMovableAnimated.hpp"
#include "types/AlphaModifiable.hpp"
#include "animationControllers/WindowAnimationController.hpp"

class CXDGSurfaceResource;
class CXWaylandSurface;
struct SXDGToplevelMoveRequest;
struct SXDGToplevelResizeRequest;
namespace Config {
    class CWorkspaceRule;
}

namespace Layout {
    class ITarget;
    class CWindowTarget;
}

namespace Desktop {
    enum eFocusReason : uint8_t;
}

namespace Desktop::View {

    class CGroup;

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
        GROUP_DENY        = 1 << 7, // deny
    };

    enum eGetWindowProperties : uint16_t {
        WINDOW_ONLY              = 0,
        RESERVED_EXTENTS         = 1 << 0,
        INPUT_EXTENTS            = 1 << 1,
        FULL_EXTENTS             = 1 << 2,
        FLOATING_ONLY            = 1 << 3,
        ALLOW_FLOATING           = 1 << 4,
        USE_PROP_TILED           = 1 << 5,
        SKIP_FULLSCREEN_PRIORITY = 1 << 6,
        FOCUS_PRIORITY           = 1 << 7,
        FOLLOW_MOUSE_CHECK       = 1 << 8,
    };

    enum eSuppressEvents : uint8_t {
        SUPPRESS_NONE                  = 0,
        SUPPRESS_FULLSCREEN            = 1 << 0,
        SUPPRESS_MAXIMIZE              = 1 << 1,
        SUPPRESS_ACTIVATE              = 1 << 2,
        SUPPRESS_ACTIVATE_FOCUSONLY    = 1 << 3,
        SUPPRESS_FULLSCREEN_OUTPUT     = 1 << 4,
        SUPPRESS_X11_CONFIGURE_REQUEST = 1 << 5,
    };

    enum eWindowAlpha : uint8_t {
        WINDOW_ALPHA_FADE = 0,
        WINDOW_ALPHA_ACTIVE,
        WINDOW_ALPHA_FULLSCREEN,
        WINDOW_ALPHA_LAYOUT,
        WINDOW_ALPHA_MOVE_TO_WORKSPACE,
        WINDOW_ALPHA_MOVE_FROM_WORKSPACE,

        WINDOW_ALPHA_LAST,
    };

    enum eWindowInputBlockReason : uint8_t {
        INPUT_BLOCK_NONE             = 0,
        INPUT_BLOCK_GROUP_INACTIVE   = (1 << 0),
        INPUT_BLOCK_MONOCLE_INACTIVE = (1 << 1),
        INPUT_BLOCK_BELOW_FULLSCREEN = (1 << 2),

        INPUT_BLOCK_ALL = std::numeric_limits<std::underlying_type_t<eWindowInputBlockReason>>::max(),
    };

    struct SWindowActiveEvent {
        PHLWINDOW    window = nullptr;
        eFocusReason reason = sc<eFocusReason>(0) /* unknown */;
    };

    struct SInitialWorkspaceToken {
        PHLWINDOWREF primaryOwner;
        std::string  workspace;
    };

    struct SFullscreenState {
        eFullscreenMode internal = FSMODE_NONE;
        eFullscreenMode client   = FSMODE_NONE;
    };

    class CWindow : public virtual IView, public virtual CGeometricMovableAnimated, public virtual IAlphaModifiable {
      public:
        static PHLWINDOW create(SP<CXDGSurfaceResource>);
        static PHLWINDOW create(SP<CXWaylandSurface>);
        static PHLWINDOW fromView(SP<IView>);

      private:
        CWindow(SP<CXDGSurfaceResource> resource);
        CWindow(SP<CXWaylandSurface> surface);

      public:
        virtual ~CWindow();

        virtual eViewType                                   type() const override;
        virtual bool                                        visible() const override;
        virtual std::optional<CBox>                         logicalBox() const override;
        virtual bool                                        desktopComponent() const override;
        virtual std::optional<CBox>                         surfaceLogicalBox() const override;
        virtual Types::CMultiAVarContainer<float, uint8_t>& alpha() override;
        virtual std::optional<uint8_t>                      alphaGenericToKey(eAlphaModifiableProp p) override;

        using CGeometricMovableAnimated::m_realPosition;
        using CGeometricMovableAnimated::m_realSize;

        struct {
            CSignalT<> destroy;
            CSignalT<> unmap;
            CSignalT<> hide;
            CSignalT<> resize;
            CSignalT<> monitorChanged;
        } m_events;

        WP<CXDGSurfaceResource> m_xdgSurface;
        WP<CXWaylandSurface>    m_xwaylandSurface;

        SP<Layout::ITarget>     m_target;

        // for not spamming the protocols
        Vector2D                                     m_reportedPosition;
        Vector2D                                     m_reportedSize;
        Vector2D                                     m_pendingReportedSize;
        std::optional<std::pair<uint32_t, Vector2D>> m_pendingSizeAck;
        std::vector<std::pair<uint32_t, Vector2D>>   m_pendingSizeAcks;

        // for floating window offset in workspace animations
        Vector2D m_floatingOffset = Vector2D(0, 0);

        // for recovering relative cursor position
        Vector2D         m_relativeCursorCoordsOnLastWarp = Vector2D(-1, -1);

        bool             m_firstMap        = false; // for layouts
        bool             m_isFloating      = false;
        SFullscreenState m_fullscreenState = {.internal = FSMODE_NONE, .client = FSMODE_NONE};
        std::string      m_title           = "";
        std::string      m_class           = "";
        std::string      m_initialTitle    = "";
        std::string      m_initialClass    = "";
        PHLWORKSPACE     m_workspace;
        PHLMONITORREF    m_monitor, m_prevMonitor;

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

        // Armed on FSMODE_FULLSCREEN exit to swallow the set_maximized that clients send to restore state.
        // Hyprland sends XDG_TOPLEVEL_STATE_MAXIMIZED to tiled windows to suppress CSD.
        // Clients echoing it back would enter FSMODE_MAXIMIZED.
        bool m_suppressNextMaximize = false;

        // desktop components
        SP<Desktop::View::CSubsurface> m_subsurfaceHead;
        SP<Desktop::View::CPopup>      m_popupHead;

        // Animated border
        Config::CGradientValueData m_realBorderColor         = {0};
        Config::CGradientValueData m_realBorderColorPrevious = {0};
        PHLANIMVAR<float>          m_borderFadeAnimationProgress;
        PHLANIMVAR<float>          m_borderAngleAnimationProgress;

        // Cached border size (invalidated by updateWindowData)
        mutable int  m_cachedBorderSize     = -1;
        mutable bool m_borderSizeCacheDirty = true;

        // Fade in-out
        bool m_animatingIn = false;

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
        UP<Desktop::Rule::CWindowRuleApplicator> m_ruleApplicator;

        // Transformers
        std::vector<UP<Render::IWindowTransformer>> m_transformers;

        // animated shadow color
        Config::CGradientValueData m_realShadowColor;
        Config::CGradientValueData m_realShadowColorPrevious;
        PHLANIMVAR<float>          m_shadowFadeAnimationProgress;
        PHLANIMVAR<float>          m_shadowAngleAnimationProgress;

        // animated glow color
        Config::CGradientValueData m_realGlowColor;
        Config::CGradientValueData m_realGlowColorPrevious;
        PHLANIMVAR<float>          m_glowFadeAnimationProgress;
        PHLANIMVAR<float>          m_glowAngleAnimationProgress;

        // animated tint
        PHLANIMVAR<float> m_dimPercent;

        // animate moving to an invisible workspace
        int m_monitorMovedFrom = -1; // -1 means not moving

        // swallowing
        PHLWINDOWREF m_swallowee;
        bool         m_currentlySwallowed = false;
        bool         m_groupSwallowed     = false;
        bool         m_hasSwallower       = false;

        // for toplevel monitor events
        MONITORID m_lastSurfaceMonitorID = -1;

        // initial token. Will be unregistered on workspace change or timeout of 2 minutes
        std::string m_initialWorkspaceToken = "";

        // for groups
        SP<CGroup> m_group;
        uint16_t   m_groupRules = Desktop::View::GROUP_NONE;

        bool       m_tearingHint = false;

        // Stable ID for ext_foreign_toplevel_list
        const uint64_t m_stableID = 0x2137;

        // ANR
        PHLANIMVAR<float> m_notRespondingTint;

        // For the noclosefor windowrule
        Time::steady_tp m_closeableSince = Time::steadyNow();

        // Desktop anim controller
        CWindowAnimationController m_animationController;

        // layout-settable flags. These are reset when layout changes.
        struct {
            bool cantLockCursor = false;
        } m_layoutFlags;

        // For the list lookup
        bool operator==(const CWindow& rhs) const;

        // methods
        CBox                   getFullWindowBoundingBox() const;
        CBox                   layoutBox() const;
        SBoxExtents            getFullWindowExtents() const;
        CBox                   getWindowBoxUnified(uint64_t props);
        SBoxExtents            getWindowExtentsUnified(uint64_t props);
        CBox                   getWindowIdealBoundingBoxIgnoreReserved();
        void                   addWindowDeco(UP<IHyprWindowDecoration> deco);
        void                   updateWindowDecos();
        void                   removeWindowDeco(IHyprWindowDecoration* deco);
        void                   uncacheWindowDecos();
        bool                   checkInputOnDecos(const eInputType, const Vector2D&, std::any = {});
        pid_t                  getPID();
        IHyprWindowDecoration* getDecorationByType(eDecorationType);
        void                   updateToplevel();
        void                   updateSurfaceScaleTransformDetails(bool force = false);
        void                   moveToWorkspace(PHLWORKSPACE);
        PHLWINDOW              x11Parent() const;
        void                   onUnmap();
        void                   onMap();
        void                   setHidden(bool hidden);
        bool                   isHidden() const;
        void                   setInputBlocked(eWindowInputBlockReason reason, bool blocked);
        /// Returns `true` if the input is blocked for this window for any reason.
        bool isInputBlocked() const;
        /// Returns `true` if any of the provided `reasons` is one of the reasons why input is blocked for this window.
        bool isInputBlockedReasonAnyOf(std::underlying_type_t<eWindowInputBlockReason> reasons) const;
        /**
         * Returns `true` if all the reasons why input is blocked for this window are contained in the provided `reason`, i.e.,
         * `reason` is the superset of reasons why input is blocked for this window.
         *
         * Note that the return value of `true` does not necessarily mean that input is blocked for all of the provided `reason`s,
         * or that input is blocked at all! If input is not blocked, the function returns `true` regardless of the argument value.
         *
         * This function is a negation of `hasInputBlockedReasonsBesides`. They exist together for the sake of readability:
         * when either of them is negated in a condition, the condition becomes hard to grasp.
         */
        bool noInputBlockedReasonsBesides(std::underlying_type_t<eWindowInputBlockReason> reason) const;
        /**
         * Returns `true` if there is a reason why input is blocked for this window that is not contained in the provided `reason`.
         *
         * Note that the return value of `false` does not mean that all the listed reasons are effective, or that the input is
         * blocked at all! If input is not blocked, the function returns `false` regardless of the argument value.
         *
         * This function is a negation of `noInputBlockedReasonsBesides`. They exist together for the sake of readability:
         * when either of them is negated in a condition, the condition becomes hard to grasp.
         */
        bool                              hasInputBlockedReasonsBesides(std::underlying_type_t<eWindowInputBlockReason> reason) const;
        bool                              acceptsInput() const;
        bool                              isAllowedOverFullscreen() const;
        bool                              isBlockedByFullscreen() const;
        bool                              isFadingOutUnderFullscreen() const;
        bool                              shouldRenderOverFullscreen() const;
        void                              updateFullscreenInputState();
        PHLANIMVAR<float>&                alpha(eWindowAlpha type);
        const PHLANIMVAR<float>&          alpha(eWindowAlpha type) const;
        float                             alphaValue(eWindowAlpha type) const;
        float                             alphaGoal(eWindowAlpha type) const;
        float                             alphaTotal() const;
        float                             alphaTotalGoal() const;
        float                             alphaTotalWithout(eWindowAlpha type) const;
        float                             effectiveAlpha() const;
        bool                              visibleByAlpha() const;
        bool                              visibleByAlphaGoal() const;
        bool                              targetVisible() const;
        void                              updateDecorationValues();
        SBoxExtents                       getFullWindowReservedArea();
        Vector2D                          middle();
        bool                              opaque();
        float                             rounding();
        float                             roundingPower();
        bool                              canBeTorn();
        void                              setSuspended(bool suspend);
        bool                              visibleOnMonitor(PHLMONITOR pMonitor);
        WORKSPACEID                       workspaceID();
        MONITORID                         monitorID();
        bool                              onSpecialWorkspace();
        void                              activate(bool force = false);
        int                               surfacesCount();
        bool                              clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize);
        bool                              isFullscreen() const;
        bool                              isEffectiveInternalFSMode(const eFullscreenMode) const;
        int                               getRealBorderSize() const;
        float                             getScrollMouse();
        float                             getScrollTouchpad();
        bool                              isScrollMouseOverridden();
        bool                              isScrollTouchpadOverridden();
        void                              updateWindowData();
        void                              updateWindowData(const Config::CWorkspaceRule&);
        void                              onBorderAngleAnimEnd(WP<Hyprutils::Animation::CBaseAnimatedVariable> pav);
        void                              onShadowAngleAnimEnd(WP<Hyprutils::Animation::CBaseAnimatedVariable> pav);
        void                              onGlowAngleAnimEnd(WP<Hyprutils::Animation::CBaseAnimatedVariable> pav);
        bool                              isInCurvedCorner(double x, double y);
        bool                              hasPopupAt(const Vector2D& pos);
        int                               popupsCount();
        void                              setAnimationsToMove();
        void                              onWorkspaceAnimUpdate();
        void                              onFocusAnimUpdate();
        std::optional<MotionBlur::SState> motionBlurState(bool allowStale = false) const;
        void                              damageMotionBlur(bool allowStale = false) const;
        void                              recordMotionBlur(const CBox& previous, const CBox& current);
        void                              resetMotionBlur();
        void                              onUpdateState();
        void                              onUpdateMeta();
        void                              onX11ConfigureRequest(CBox box);
        void                              onResourceChangeX11();
        std::string                       fetchTitle();
        std::string                       fetchClass();
        void                              warpCursor(bool force = false);
        PHLWINDOW                         getSwallowee();
        bool                              isX11OverrideRedirect();
        bool                              isModal();
        Vector2D                          realToReportSize();
        Vector2D                          realToReportPosition();
        Vector2D                          xwaylandSizeToReal(Vector2D size);
        Vector2D                          xwaylandPositionToReal(Vector2D size);
        void                              updateX11SurfaceScale();
        void                              sendWindowSize(bool force = false);
        NContentType::eContentType        getContentType();
        void                              setContentType(NContentType::eContentType contentType);
        void                              deactivateGroupMembers();
        bool                              isNotResponding();
        std::optional<std::string>        xdgTag();
        std::optional<std::string>        xdgDescription();
        PHLWINDOW                         parent();
        bool                              priorityFocus();
        SP<CWLSurfaceResource>            getSolitaryResource();
        Vector2D                          getReportedSize();
        std::optional<Vector2D>           calculateExpression(const std::string& s);
        std::optional<Vector2D>           calculateExpression(const Math::SExpressionVec2& expr);
        std::optional<Vector2D>           minSize();
        std::optional<Vector2D>           maxSize();
        SP<Layout::ITarget>               layoutTarget();
        bool                              canBeGroupedInto(SP<CGroup> group);
        void                              sendClose();

        CBox                              getWindowMainSurfaceBox() const {
            return geometricBox(GEOMETRIC_CURRENT);
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
            CHyprSignalListener xdgMoveRequest;
            CHyprSignalListener xdgResizeRequest;
        } m_listeners;

      private:
        std::optional<double> calculateSingleExpr(const std::string& s);
        void                  mapWindow();
        void                  unmapWindow();
        void                  commitWindow();
        void                  destroyWindow();
        void                  activateX11();
        void                  onXDGMoveRequest(const SXDGToplevelMoveRequest& request);
        void                  onXDGResizeRequest(const SXDGToplevelResizeRequest& request);
        void                  unmanagedSetGeometry();

        // For hidden windows and stuff
        bool                                                                             m_hidden            = false;
        bool                                                                             m_suspended         = false;
        WORKSPACEID                                                                      m_lastWorkspace     = WORKSPACE_INVALID;
        uint32_t                                                                         m_inputBlockReasons = INPUT_BLOCK_NONE;
        Desktop::Types::CMultiAVarContainer<float, std::underlying_type_t<eWindowAlpha>> m_alpha;
    };

    inline bool valid(const PHLWINDOW& w) {
        return w.get();
    }

    inline bool valid(const PHLWINDOWREF& w) {
        return !w.expired();
    }

    inline bool validMapped(const PHLWINDOW& w) {
        if (!valid(w))
            return false;
        return w->m_isMapped;
    }

    inline bool validMapped(const PHLWINDOWREF& w) {
        if (!valid(w))
            return false;
        return w->m_isMapped;
    }
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
            return std::format_to(out, "{:x}", rc<uintptr_t>(w.get()));
        if (!w)
            return std::format_to(out, "[Window nullptr]");

        std::format_to(out, "[");
        std::format_to(out, "Window {:x}: title: \"{}\"", rc<uintptr_t>(w.get()), w->m_title);
        if (formatWorkspace)
            std::format_to(out, ", workspace: {}", w->m_workspace ? w->workspaceID() : WORKSPACE_INVALID);
        if (formatMonitor)
            std::format_to(out, ", monitor: {}", w->monitorID());
        if (formatClass)
            std::format_to(out, ", class: {}", w->m_class);
        return std::format_to(out, "]");
    }
};
