#pragma once

#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

#include "../View.hpp"
#include "../../../config/shared/complex/ComplexDataTypes.hpp"
#include "../../../macros/Enums.hpp"
#include "../../../helpers/AnimatedVariable.hpp"
#include "../../../macros.hpp"
#include "../../DesktopTypes.hpp"
#include "../../types/MultiAnimatedVariable.hpp"
#include "../WLSurface.hpp"
#include "../../../workspace/HLWorkspace.hpp"
#include "../../rule/windowRule/WindowRuleApplicator.hpp"
#include "../../../protocols/types/ContentType.hpp"
#include "../types/GeometricMovableAnimated.hpp"
#include "../types/AlphaModifiable.hpp"
#include "../focusable/Focusable.hpp"
#include "../surfaceTree/PopupOwner.hpp"
#include "../surfaceTree/SubsurfaceOwner.hpp"
#include "WindowBackend.hpp"

namespace Config {
    class CWorkspaceRule;
}

namespace Layout {
    class ITarget;
    class CWindowTarget;
}

namespace Desktop {
    enum eFocusReason : uint32_t;
}

namespace Desktop::View {

    class CGroup;
    class CWindowEffectsController;
    class CWindowFullscreenPolicy;
    class CWindowGroupMembership;
    class CWindowMetadata;
    class CWindowPresentation;
    class CWindowSwallowController;

    enum eWindowUpdateSource : uint8_t {
        WINDOW_UPDATE_ANIMATION = 0,
        WINDOW_UPDATE_MOUSE,
        WINDOW_UPDATE_LAYOUT,
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

    enum eWindowAlpha : uint8_t {
        WINDOW_ALPHA_FADE = 0,
        WINDOW_ALPHA_ACTIVE,
        WINDOW_ALPHA_FULLSCREEN,
        WINDOW_ALPHA_LAYOUT,
        WINDOW_ALPHA_MOVE_TO_WORKSPACE,
        WINDOW_ALPHA_MOVE_FROM_WORKSPACE,

        WINDOW_ALPHA_LAST,
    };

    enum class eWindowState : uint8_t {
        WINDOW_STATE_NONE             = 0,
        WINDOW_STATE_PINNED           = (1 << 0),
        WINDOW_STATE_NO_INITIAL_FOCUS = (1 << 1),
        WINDOW_STATE_FIRST_MAP        = (1 << 2),
    };

    using enum eWindowState;
    EXPOSE_ENUM_AS_MASK(eWindowState, WindowState);

    enum class eWindowHints : uint8_t {
        WINDOW_HINT_NONE   = 0,
        WINDOW_HINT_URGENT = (1 << 0),
        WINDOW_HINT_TEAR   = (1 << 1),
    };

    using enum eWindowHints;
    EXPOSE_ENUM_AS_MASK(eWindowHints, WindowHints);

    struct SWindowActiveEvent {
        PHLWINDOW    window = nullptr;
        eFocusReason reason = sc<eFocusReason>(0) /* unknown */;
    };

    struct SInitialWorkspaceToken {
        PHLWINDOWREF              primaryOwner;
        Workspace::WorkspaceID    workspaceID;
        std::string               workspaceAddress;
        Workspace::eWorkspaceType workspaceType = Workspace::eWorkspaceType::NORMAL;
    };

    struct SClientFullscreenRequest {
        std::optional<bool>      fullscreen;
        std::optional<bool>      maximized;
        std::optional<MONITORID> fullscreenMonitor;
        enum eOrigin : uint8_t {
            ORIGIN_BACKEND,
            ORIGIN_FOREIGN_TOPLEVEL,
        } origin = ORIGIN_BACKEND;
    };

    class CWindow : public virtual IView, public virtual CGeometricMovableAnimated, public virtual IAlphaModifiable, public virtual CPopupOwner, public virtual CSubsurfaceOwner {
      public:
        static PHLWINDOW create(UP<IWindowBackend> backend);
        static PHLWINDOW fromView(SP<IView>);

      private:
        CWindow(UP<IWindowBackend> backend);

      public:
        virtual ~CWindow();

        IWindowBackend&                                           backend();
        const IWindowBackend&                                     backend() const;
        CWindowEffectsController&                                 effects();
        const CWindowEffectsController&                           effects() const;
        const CWindowMetadata&                                    metadata() const;
        CWindowPresentation&                                      presentation();
        const CWindowPresentation&                                presentation() const;
        CWindowSwallowController&                                 swallowing();
        const CWindowSwallowController&                           swallowing() const;
        CWindowFullscreenPolicy&                                  fullscreenPolicy();
        const CWindowFullscreenPolicy&                            fullscreenPolicy() const;
        CWindowGroupMembership&                                   grouping();
        const CWindowGroupMembership&                             grouping() const;

        virtual eViewType                                         type() const override;
        virtual bool                                              mapped() const override;
        virtual bool                                              focusAvailable() const override;
        virtual std::optional<CBox>                               logicalBox() const override;
        virtual bool                                              desktopComponent() const override;
        virtual std::optional<CBox>                               surfaceLogicalBox() const override;
        virtual Types::CMultiAVarContainer<float, uint8_t>&       alpha() override;
        virtual const Types::CMultiAVarContainer<float, uint8_t>& alpha() const override;
        virtual std::optional<uint8_t>                            alphaGenericToKey(eAlphaModifiableProp p) override;

        struct {
            CSignalT<> destroy;
            CSignalT<> unmap;
            CSignalT<> hide;
            CSignalT<> resize;
            CSignalT<> monitorChanged;
        } m_events;

        WindowHints m_hints = WINDOW_HINT_NONE;
        WindowState m_state = WINDOW_STATE_NONE;

        // for recovering relative cursor position
        Vector2D      m_relativeCursorCoordsOnLastWarp = Vector2D(-1, -1);

        PHLWORKSPACE  m_workspace;
        PHLMONITORREF m_monitor, m_prevMonitor;

        // for proper cycling. While cycling we can't just move the pointers, so we need to keep track of the last cycled window.
        PHLWINDOWREF m_lastCycledWindow;

        // Special render data, rules, etc
        UP<Desktop::Rule::CWindowRuleApplicator> m_ruleApplicator;

        // for toplevel monitor events
        MONITORID m_lastSurfaceMonitorID = -1;

        // initial token. Will be unregistered on workspace change or timeout of 2 minutes
        std::string m_initialWorkspaceToken = "";

        // For the noclosefor windowrule
        Time::steady_tp m_closeableSince = Time::steadyNow();

        // For the list lookup
        bool operator==(const CWindow& rhs) const;

        // methods
        CBox                       getFullWindowBoundingBox() const;
        CBox                       layoutBox() const;
        SBoxExtents                getFullWindowExtents() const;
        CBox                       getWindowBoxUnified(uint64_t props);
        SBoxExtents                getWindowExtentsUnified(uint64_t props);
        CBox                       getWindowIdealBoundingBoxIgnoreReserved();
        void                       updateToplevel();
        void                       updateSurfaceScaleTransformDetails(bool force = false);
        void                       moveToWorkspace(PHLWORKSPACE);
        void                       onUnmap();
        void                       onMap();
        void                       setHidden(bool hidden);
        bool                       isHidden() const;
        bool                       shouldBlur() const;
        bool                       isAllowedOverFullscreen() const;
        bool                       isBlockedByFullscreen() const;
        bool                       isFadingOutUnderFullscreen() const;
        bool                       shouldRenderOverFullscreen() const;
        void                       updateFullscreenInputState();
        SBoxExtents                getFullWindowReservedArea();
        Vector2D                   middle();
        bool                       canBeTorn();
        void                       setSuspended(bool suspend);
        MONITORID                  monitorID();
        bool                       onSpecialWorkspace();
        const std::string&         workspaceAddress() const;
        std::string_view           workspaceType() const;
        void                       activate(bool force = false);
        bool                       clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize);
        float                      getScrollMouse();
        float                      getScrollTouchpad();
        bool                       isScrollMouseOverridden();
        bool                       isScrollTouchpadOverridden();
        void                       updateWindowData();
        void                       updateWindowData(const Config::CWorkspaceRule&);
        void                       warpCursor(bool force = false);
        bool                       shouldntFocus() const;
        bool                       suggestsFloat(bool pending = false) const;
        void                       acknowledgeClientGeometry(const CBox& logicalBox);
        void                       sendWindowSize(bool force = false);
        NContentType::eContentType getContentType();
        void                       setContentType(NContentType::eContentType contentType);
        void                       deactivateGroupMembers();
        bool                       isNotResponding();
        bool                       priorityFocus();
        SP<CWLSurfaceResource>     getSolitaryResource() const;
        std::optional<Vector2D>    calculateExpression(const Math::SExpressionVec2& expr);
        std::optional<Vector2D>    minSize();
        std::optional<Vector2D>    maxSize();
        SP<Layout::CWindowTarget>  windowTarget();
        SP<Layout::CWindowTarget>  windowTarget() const;
        // Returns the effective target, e.g. the group target for a grouped window.
        SP<Layout::ITarget> layoutTarget();
        SP<Layout::ITarget> layoutTarget() const;
        bool                isFloating() const;
        bool                cantLockCursor() const;
        void                sendClose();
        void                requestClientFullscreen(const SClientFullscreenRequest& request);

        CBox                getWindowMainSurfaceBox() const {
            return geometricBox(GEOMETRIC_CURRENT);
        }

        std::unordered_map<std::string, std::string> getEnv();

        //
        PHLWINDOWREF m_self;

      private:
        void         initialize();
        void         attachBackendListeners();
        void         mapWindow();
        void         unmapWindow();
        void         commitWindow(bool initialCommit);
        void         destroyWindow();
        void         onUpdateState(const SBackendStateRequest& request);
        void         onUpdateMeta(const SBackendMetadata& metadata);
        void         onSurfaceChanged(SP<CWLSurfaceResource> surface);
        void         onConfigureRequest(const CBox& box);
        void         onGeometryChanged(const CBox& box);
        void         onActivationRequest();
        void         onMoveRequest();
        void         onResizeRequest(eBackendResizeEdge edge);
        void         unmanagedSetGeometry(const CBox& box);
        virtual void onInputBlockStateUpdated(bool blocked) override;
        // For hidden windows and stuff
        bool        m_hidden    = false;
        bool        m_suspended = false;
        bool        m_isMapped  = false;
        std::string m_lastWorkspaceAddress;
        std::string m_lastWorkspaceType;
        bool        m_lastWorkspaceSpecial = false;

        struct {
            bool activate            = false;
            bool activateFocusOnly   = false;
            bool x11ConfigureRequest = false;
        } m_requestSuppression;

        // Listeners must be destroyed before the backend that owns their signals.
        UP<CWindowGroupMembership>   m_grouping;
        UP<CWindowSwallowController> m_swallowing;
        UP<CWindowFullscreenPolicy>  m_fullscreenPolicy;
        UP<IWindowBackend>           m_backend;
        UP<CWindowMetadata>          m_metadata;
        UP<CWindowPresentation>      m_presentation;
        UP<CWindowEffectsController> m_effects;
        SP<Layout::CWindowTarget>    m_target;
        struct {
            CHyprSignalListener map;
            CHyprSignalListener unmap;
            CHyprSignalListener commit;
            CHyprSignalListener destroy;
            CHyprSignalListener surfaceChanged;
            CHyprSignalListener metadataChanged;
            CHyprSignalListener stateRequest;
            CHyprSignalListener configureRequest;
            CHyprSignalListener geometryChanged;
            CHyprSignalListener activationRequest;
            CHyprSignalListener moveRequest;
            CHyprSignalListener resizeRequest;
            CHyprSignalListener newPopup;
        } m_backendListeners;
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
        return w->mapped();
    }

    inline bool validMapped(const PHLWINDOWREF& w) {
        if (!valid(w))
            return false;
        return w->mapped();
    }
}

#include "WindowMetadata.hpp"

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
        std::format_to(out, "Window {:x}: title: \"{}\"", rc<uintptr_t>(w.get()), w->metadata().title());
        if (formatWorkspace)
            std::format_to(out, ", workspace: {}", w->m_workspace ? w->m_workspace->addressableName() : "none");
        if (formatMonitor)
            std::format_to(out, ", monitor: {}", w->monitorID());
        if (formatClass)
            std::format_to(out, ", class: {}", w->metadata().appID());
        return std::format_to(out, "]");
    }
};
