#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>
#include <hyprutils/animation/AnimatedVariable.hpp>

#include "../Group.hpp"

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <any>
#include <bit>
#include <fstream>
#include <string_view>
#include "Window.hpp"
#include "WindowEffectsController.hpp"
#include "WindowFullscreenPolicy.hpp"
#include "WindowGroupMembership.hpp"
#include "WindowMetadata.hpp"
#include "WindowPresentation.hpp"
#include "WindowSwallowController.hpp"
#include "../LayerSurface.hpp"
#include "../Popup.hpp"
#include "../Subsurface.hpp"
#include "../../state/FocusState.hpp"
#include "../../state/FloatState.hpp"
#include "../../state/FadingOutState.hpp"
#include "../../state/GlobalWindowController.hpp"
#include "../../state/WindowFadeout.hpp"
#include "../../state/WindowState.hpp"
#include "../../history/WindowHistoryTracker.hpp"
#include "../../../Compositor.hpp"
#include "../../../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../config/shared/actions/ConfigActions.hpp"
#include "../../../config/ConfigManager.hpp"
#include "../../../config/shared/animation/AnimationTree.hpp"
#include "../../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../../../state/MonitorState.hpp"
#include "../../../state/WorkspaceState.hpp"
#include "../../../managers/TokenManager.hpp"
#include "../../../animation/AnimationManager.hpp"
#include "../../../managers/ANRManager.hpp"
#include "../../../managers/eventLoop/EventLoopManager.hpp"
#include "../../../managers/eventLoop/EventLoopTimer.hpp"
#include "../../../protocols/core/Compositor.hpp"
#include "../../../protocols/core/Subcompositor.hpp"
#include "../../../protocols/ContentType.hpp"
#include "../../../protocols/LayerShell.hpp"
#include "../../../helpers/Color.hpp"
#include "../../../helpers/math/Expression.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../ipc/s2/S2.hpp"
#include "../../../managers/input/InputManager.hpp"
#include "../../../pointer/PointerController.hpp"
#include "../../../managers/fullscreen/FullscreenController.hpp"
#include "../../../layout/algorithm/Algorithm.hpp"
#include "../../../layout/space/Space.hpp"
#include "../../../layout/LayoutManager.hpp"
#include "../../../layout/target/WindowTarget.hpp"
#include "../../../layout/target/WindowGroupTarget.hpp"
#include "../../../event/EventBus.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/VarList2.hpp>

using namespace Hyprutils::String;
using namespace Hyprutils::Animation;
using enum NContentType::eContentType;

using namespace Desktop;
using namespace Desktop::View;

static Layout::eRectCorner backendResizeEdgeToCorner(eBackendResizeEdge edge) {
    switch (edge) {
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_TOP: return Layout::CORNER_TOP;
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_BOTTOM: return Layout::CORNER_BOTTOM;
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_LEFT: return Layout::CORNER_LEFT;
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_TOP_LEFT: return Layout::CORNER_TOPLEFT;
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_BOTTOM_LEFT: return Layout::CORNER_BOTTOMLEFT;
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_RIGHT: return Layout::CORNER_RIGHT;
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_TOP_RIGHT: return Layout::CORNER_TOPRIGHT;
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_BOTTOM_RIGHT: return Layout::CORNER_BOTTOMRIGHT;
        case eBackendResizeEdge::BACKEND_RESIZE_EDGE_NONE: return Layout::CORNER_NONE;
    }

    return Layout::CORNER_NONE;
}

//
#define COMMA ,
//

PHLWINDOW CWindow::create(UP<IWindowBackend> backend) {
    if (!backend)
        return nullptr;

    PHLWINDOW pWindow = SP<CWindow>(new CWindow(std::move(backend)));

    pWindow->m_self = pWindow;
    pWindow->m_backend->attach(pWindow);
    pWindow->initialize();
    pWindow->attachBackendListeners();

    return pWindow;
}

void CWindow::initialize() {
    const auto SELF = m_self.lock();

    m_ruleApplicator = makeUnique<Desktop::Rule::CWindowRuleApplicator>(SELF);

    Animation::mgr()->createAnimation(Vector2D(0, 0), positionAnimation(), Config::animationTree()->getAnimationPropertyConfig("windowsIn"), SELF, AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(Vector2D(0, 0), sizeAnimation(), Config::animationTree()->getAnimationPropertyConfig("windowsIn"), SELF, AVARDAMAGE_ENTIRE);
    m_presentation->initialize();

    m_target = Layout::CWindowTarget::create(SELF);
    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_MAPPED);

    if (const auto SURFACE = m_backend->surface())
        wlSurface()->assign(SURFACE, SELF);

    initView(SELF, VIEW_TYPE_WINDOW);
    Event::bus()->m_events.window.create.emit(SELF);
}

void CWindow::attachBackendListeners() {
    m_backendListeners.map               = m_backend->m_events.map.listen([this] { mapWindow(); });
    m_backendListeners.unmap             = m_backend->m_events.unmap.listen([this] { unmapWindow(); });
    m_backendListeners.commit            = m_backend->m_events.commit.listen([this](bool initial) { commitWindow(initial); });
    m_backendListeners.destroy           = m_backend->m_events.destroy.listen([this] { destroyWindow(); });
    m_backendListeners.surfaceChanged    = m_backend->m_events.surfaceChanged.listen([this](const auto& surface) { onSurfaceChanged(surface); });
    m_backendListeners.metadataChanged   = m_backend->m_events.metadataChanged.listen([this](const auto& metadata) { onUpdateMeta(metadata); });
    m_backendListeners.stateRequest      = m_backend->m_events.stateRequest.listen([this](const auto& request) { onUpdateState(request); });
    m_backendListeners.configureRequest  = m_backend->m_events.configureRequest.listen([this](const auto& box) { onConfigureRequest(box); });
    m_backendListeners.geometryChanged   = m_backend->m_events.geometryChanged.listen([this](const auto& box) { onGeometryChanged(box); });
    m_backendListeners.activationRequest = m_backend->m_events.activationRequest.listen([this] { onActivationRequest(); });
    m_backendListeners.moveRequest       = m_backend->m_events.moveRequest.listen([this] { onMoveRequest(); });
    m_backendListeners.resizeRequest     = m_backend->m_events.resizeRequest.listen([this](eBackendResizeEdge edge) { onResizeRequest(edge); });
    m_backendListeners.newPopup          = m_backend->m_events.newPopup.listen([this](const auto& popup) {
        if (popupHead())
            popupHead()->onNewPopup(popup);
    });
}

CWindow::CWindow(UP<IWindowBackend> backend) :
    IView(CWLSurface::create()), m_grouping(makeUnique<CWindowGroupMembership>(*this)), m_swallowing(makeUnique<CWindowSwallowController>(*this)),
    m_fullscreenPolicy(makeUnique<CWindowFullscreenPolicy>()), m_backend(std::move(backend)), m_metadata(makeUnique<CWindowMetadata>()),
    m_presentation(makeUnique<CWindowPresentation>(*this)), m_effects(makeUnique<CWindowEffectsController>(*this)) {
    ;
}

SP<CWindow> CWindow::fromView(SP<IView> v) {
    if (!v || v->type() != VIEW_TYPE_WINDOW)
        return nullptr;
    return dynamicPointerCast<CWindow>(v);
}

CWindow::~CWindow() {
    m_swallowing->onDestroy();

    if (Desktop::focusState()->window() == m_self) {
        Desktop::focusState()->surface().reset();
        Desktop::focusState()->window().reset();
    }

    Event::bus()->m_events.window.destroy.emit(m_self);
    m_events.destroy.emit();
}

IWindowBackend& CWindow::backend() {
    return *m_backend;
}

const IWindowBackend& CWindow::backend() const {
    return *m_backend;
}

CWindowSwallowController& CWindow::swallowing() {
    return *m_swallowing;
}

const CWindowSwallowController& CWindow::swallowing() const {
    return *m_swallowing;
}

CWindowFullscreenPolicy& CWindow::fullscreenPolicy() {
    return *m_fullscreenPolicy;
}

const CWindowFullscreenPolicy& CWindow::fullscreenPolicy() const {
    return *m_fullscreenPolicy;
}

CWindowEffectsController& CWindow::effects() {
    return *m_effects;
}

const CWindowEffectsController& CWindow::effects() const {
    return *m_effects;
}

const CWindowMetadata& CWindow::metadata() const {
    return *m_metadata;
}

CWindowPresentation& CWindow::presentation() {
    return *m_presentation;
}

const CWindowPresentation& CWindow::presentation() const {
    return *m_presentation;
}

eViewType CWindow::type() const {
    return VIEW_TYPE_WINDOW;
}

bool CWindow::mapped() const {
    return m_isMapped;
}

bool CWindow::focusAvailable() const {
    return !isHidden();
}

std::optional<CBox> CWindow::logicalBox() const {
    return getFullWindowBoundingBox();
}

bool CWindow::desktopComponent() const {
    return true;
}

std::optional<CBox> CWindow::surfaceLogicalBox() const {
    if (!mapped() || !acceptsInput() || !alphaNonZero())
        return std::nullopt;

    return getWindowMainSurfaceBox();
}

SBoxExtents CWindow::getFullWindowExtents() const {
    const int BORDERSIZE = m_presentation->borderSize();

    if (m_ruleApplicator->dimAround().valueOrDefault()) {
        if (const auto PMONITOR = m_monitor.lock(); PMONITOR)
            return {.topLeft     = {m_realPosition->value().x - PMONITOR->m_position.x, m_realPosition->value().y - PMONITOR->m_position.y},
                    .bottomRight = {PMONITOR->m_size.x - (m_realPosition->value().x - PMONITOR->m_position.x),
                                    PMONITOR->m_size.y - (m_realPosition->value().y - PMONITOR->m_position.y)}};
    }

    SBoxExtents maxExtents = {.topLeft = {BORDERSIZE + 2, BORDERSIZE + 2}, .bottomRight = {BORDERSIZE + 2, BORDERSIZE + 2}};

    const auto  EXTENTS = g_pDecorationPositioner->getWindowDecorationExtents(m_self);

    maxExtents.topLeft.x = std::max(EXTENTS.topLeft.x, maxExtents.topLeft.x);

    maxExtents.topLeft.y = std::max(EXTENTS.topLeft.y, maxExtents.topLeft.y);

    maxExtents.bottomRight.x = std::max(EXTENTS.bottomRight.x, maxExtents.bottomRight.x);

    maxExtents.bottomRight.y = std::max(EXTENTS.bottomRight.y, maxExtents.bottomRight.y);

    if (m_wlSurface->exists() && !m_backend->isX11() && popupHead()) {
        const auto& surfaceExtents = popupHead()->popupTreeExtents();

        maxExtents.topLeft.x = std::max(-surfaceExtents.x, maxExtents.topLeft.x);

        maxExtents.topLeft.y = std::max(-surfaceExtents.y, maxExtents.topLeft.y);

        if (surfaceExtents.x + surfaceExtents.width > m_wlSurface->resource()->m_current.size.x + maxExtents.bottomRight.x)
            maxExtents.bottomRight.x = surfaceExtents.x + surfaceExtents.width - m_wlSurface->resource()->m_current.size.x;

        if (surfaceExtents.y + surfaceExtents.height > m_wlSurface->resource()->m_current.size.y + maxExtents.bottomRight.y)
            maxExtents.bottomRight.y = surfaceExtents.y + surfaceExtents.height - m_wlSurface->resource()->m_current.size.y;
    }

    return maxExtents;
}

CBox CWindow::getFullWindowBoundingBox() const {
    if (m_ruleApplicator->dimAround().valueOrDefault()) {
        if (const auto PMONITOR = m_monitor.lock(); PMONITOR)
            return {PMONITOR->m_position.x, PMONITOR->m_position.y, PMONITOR->m_size.x, PMONITOR->m_size.y};
    }

    auto maxExtents = getFullWindowExtents();

    CBox finalBox = geometricBox(GEOMETRIC_CURRENT);
    finalBox.addExtents(maxExtents);

    return finalBox;
}

bool CWindow::operator==(const CWindow& rhs) const {
    return this == &rhs && layoutBox() == rhs.layoutBox();
}

CBox CWindow::layoutBox() const {
    if (!m_target)
        return {};

    return m_target->position();
}

CBox CWindow::getWindowIdealBoundingBoxIgnoreReserved() {
    const auto PMONITOR = m_monitor.lock();

    const auto LAYOUTBOX = layoutBox();

    if (!PMONITOR || !m_workspace)
        return LAYOUTBOX;

    auto POS  = LAYOUTBOX.pos();
    auto SIZE = LAYOUTBOX.size();

    if (Fullscreen::controller()->isFullscreen(m_self.lock()) && (!layoutTarget() || !Fullscreen::controller()->layoutManagedFS(m_self.lock()))) {
        POS  = PMONITOR->m_position;
        SIZE = PMONITOR->m_size;

        return CBox{sc<int>(POS.x), sc<int>(POS.y), sc<int>(SIZE.x), sc<int>(SIZE.y)};
    }

    // fucker fucking fuck
    const auto  WORKAREA = m_workspace->m_space->workArea();
    const auto& RESERVED = CReservedArea(PMONITOR->logicalBox(), WORKAREA);

    if (!RESERVED.ok())
        return CBox{POS, SIZE};

    if (DELTALESSTHAN(POS.x, WORKAREA.x, 1)) {
        POS.x -= RESERVED.left();
        SIZE.x += RESERVED.left();
    }

    if (DELTALESSTHAN(POS.y, WORKAREA.y, 1)) {
        POS.y -= RESERVED.top();
        SIZE.y += RESERVED.top();
    }

    if (DELTALESSTHAN(POS.x + SIZE.x, WORKAREA.x + WORKAREA.width, 1))
        SIZE.x += RESERVED.right();

    if (DELTALESSTHAN(POS.y + SIZE.y, WORKAREA.y + WORKAREA.height, 1))
        SIZE.y += RESERVED.bottom();

    return CBox{sc<int>(POS.x), sc<int>(POS.y), sc<int>(SIZE.x), sc<int>(SIZE.y)};
}

SBoxExtents CWindow::getWindowExtentsUnified(uint64_t properties) {
    SBoxExtents extents = {.topLeft = {0, 0}, .bottomRight = {0, 0}};
    if (properties & Desktop::View::RESERVED_EXTENTS)
        extents.addExtents(g_pDecorationPositioner->getWindowDecorationReserved(m_self));
    if (properties & Desktop::View::INPUT_EXTENTS)
        extents.addExtents(g_pDecorationPositioner->getWindowDecorationExtents(m_self, true));
    if (properties & FULL_EXTENTS)
        extents.addExtents(g_pDecorationPositioner->getWindowDecorationExtents(m_self, false));

    return extents;
}

CBox CWindow::getWindowBoxUnified(uint64_t properties) {
    if (m_ruleApplicator->dimAround().valueOrDefault()) {
        const auto PMONITOR = m_monitor.lock();
        if (PMONITOR)
            return {PMONITOR->m_position.x, PMONITOR->m_position.y, PMONITOR->m_size.x, PMONITOR->m_size.y};
    }

    CBox box = geometricBox(GEOMETRIC_CURRENT);
    box.addExtents(getWindowExtentsUnified(properties));

    return box;
}

SBoxExtents CWindow::getFullWindowReservedArea() {
    return g_pDecorationPositioner->getWindowDecorationReserved(m_self);
}

void CWindow::updateToplevel() {
    updateSurfaceScaleTransformDetails();
}

void CWindow::updateSurfaceScaleTransformDetails(bool force) {
    if (!m_isMapped || m_hidden)
        return;

    const auto PLASTMONITOR = State::monitorState()->query().id(m_lastSurfaceMonitorID).run();

    m_lastSurfaceMonitorID = monitorID();

    const auto PNEWMONITOR = m_monitor.lock();

    if (!PNEWMONITOR)
        return;

    if (PNEWMONITOR != PLASTMONITOR || force) {
        if (PLASTMONITOR && PLASTMONITOR->m_enabled && PNEWMONITOR != PLASTMONITOR)
            m_wlSurface->resource()->breadthfirst([PLASTMONITOR](SP<CWLSurfaceResource> s, const Vector2D& offset, void* d) { s->leave(PLASTMONITOR->m_self.lock()); }, nullptr);

        m_wlSurface->resource()->breadthfirst([PNEWMONITOR](SP<CWLSurfaceResource> s, const Vector2D& offset, void* d) { s->enter(PNEWMONITOR->m_self.lock()); }, nullptr);
    }

    const auto PMONITOR = m_monitor.lock();

    m_wlSurface->resource()->breadthfirst(
        [PMONITOR](SP<CWLSurfaceResource> s, const Vector2D& offset, void* d) {
            const auto PSURFACE = CWLSurface::fromResource(s);

            if (!PSURFACE)
                return;

            PSURFACE->sendScale(PMONITOR->m_scale);
            PSURFACE->sendTransform(PMONITOR->m_transform);
        },
        nullptr);
}

void CWindow::moveToWorkspace(PHLWORKSPACE pWorkspace) {
    if (m_workspace == pWorkspace)
        return;

    static auto PINITIALWSTRACKING = CConfigValue<Config::INTEGER>("misc:initial_workspace_tracking");

    if (!m_initialWorkspaceToken.empty()) {
        const auto TOKEN = g_pTokenManager->getToken(m_initialWorkspaceToken);
        if (TOKEN) {
            if (*PINITIALWSTRACKING == 2) {
                // persistent
                try {
                    SInitialWorkspaceToken token = std::any_cast<SInitialWorkspaceToken>(TOKEN->m_data);
                    if (token.primaryOwner == m_self) {
                        token.workspace = pWorkspace->getConfigName();
                        TOKEN->m_data   = token;
                    }
                } catch (const std::bad_any_cast& e) { ; }
            }
        }
    }

    static auto PCLOSEONLASTSPECIAL = CConfigValue<Config::INTEGER>("misc:close_special_on_empty");

    const auto  OLDWORKSPACE = m_workspace;

    if (OLDWORKSPACE->isVisible()) {
        m_presentation->alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE)->setValueAndWarp(1.F);
        *m_presentation->alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE) = 0.F;
        m_presentation->alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE)->setCallbackOnEnd([this](auto) {
            m_presentation->alpha(WINDOW_ALPHA_MOVE_TO_WORKSPACE)->setValueAndWarp(1.F);
            m_presentation->resetMonitorMovedFrom();
        });
        m_presentation->setMonitorMovedFrom(OLDWORKSPACE ? OLDWORKSPACE->monitorID() : -1);
    }

    m_workspace = pWorkspace;
    updateFullscreenInputState();
    *m_presentation->alpha(WINDOW_ALPHA_FULLSCREEN) = isBlockedByFullscreen() ? 0.F : 1.F;

    m_presentation->setAnimationsToMove();

    Desktop::globalWindowController()->updateAllWindowsDecorations();

    if (valid(pWorkspace)) {
        IPC::Socket2::sock()->postEvent({.event = "movewindow", .data = std::format("{:x},{}", rc<uintptr_t>(this), pWorkspace->m_name)});
        IPC::Socket2::sock()->postEvent({.event = "movewindowv2", .data = std::format("{:x},{},{}", rc<uintptr_t>(this), pWorkspace->m_id, pWorkspace->m_name)});
        Event::bus()->m_events.window.moveToWorkspace.emit(m_self.lock(), pWorkspace);
    }

    m_swallowing->moveToWorkspace(pWorkspace);

    if (OLDWORKSPACE && State::workspaceState()->isSpecial(OLDWORKSPACE->m_id) && OLDWORKSPACE->getWindowCount() == 0 && *PCLOSEONLASTSPECIAL) {
        if (const auto PMONITOR = OLDWORKSPACE->m_monitor.lock(); PMONITOR)
            PMONITOR->setSpecialWorkspace(nullptr);
    }
}

void CWindow::onUnmap() {
    static auto PCLOSEONLASTSPECIAL = CConfigValue<Config::INTEGER>("misc:close_special_on_empty");
    static auto PINITIALWSTRACKING  = CConfigValue<Config::INTEGER>("misc:initial_workspace_tracking");

    if (!m_initialWorkspaceToken.empty()) {
        const auto TOKEN = g_pTokenManager->getToken(m_initialWorkspaceToken);
        if (TOKEN) {
            if (*PINITIALWSTRACKING == 2) {
                // persistent token, but the first window got removed so the token is gone
                try {
                    SInitialWorkspaceToken token = std::any_cast<SInitialWorkspaceToken>(TOKEN->m_data);
                    if (token.primaryOwner == m_self)
                        g_pTokenManager->removeToken(TOKEN);
                } catch (const std::bad_any_cast& e) { g_pTokenManager->removeToken(TOKEN); }
            }
        }
    }

    m_lastWorkspace = m_workspace->m_id;

    // if the special workspace now has 0 windows, it will be closed, and this
    // window will no longer pass render checks, cuz the workspace will be nuked.
    // throw it into the main one for the fadeout.
    if (m_workspace->m_isSpecialWorkspace && m_workspace->getWindowCount() == 0) {
        const auto PMONITOR = m_monitor.lock();
        if (PMONITOR)
            m_lastWorkspace = PMONITOR->activeWorkspaceID();
    }

    if (*PCLOSEONLASTSPECIAL && m_workspace && m_workspace->getWindowCount() == 0 && onSpecialWorkspace()) {
        const auto PMONITOR = m_monitor.lock();
        if (PMONITOR && PMONITOR->m_activeSpecialWorkspace && PMONITOR->m_activeSpecialWorkspace == m_workspace)
            PMONITOR->setSpecialWorkspace(nullptr);
    }

    const auto PMONITOR = m_monitor.lock();

    if (PMONITOR && PMONITOR->m_solitaryClient == m_self)
        PMONITOR->m_solitaryClient.reset();

    m_effects->reset();

    if (m_workspace) {
        m_workspace->updateWindows();
        m_workspace->updateWindowData();
    }

    Desktop::globalWindowController()->updateAllWindowsDecorations();

    m_workspace.reset();

    if (m_backend->isX11())
        return;

    resetSubsurfaceHead();
    resetPopupHead();
}

void CWindow::onMap() {
    // JIC, reset the callbacks. If any are set, we'll make sure they are cleared so we don't accidentally unset them. (In case a window got remapped)
    m_realPosition->resetAllCallbacks();
    m_realSize->resetAllCallbacks();
    m_presentation->prepareMap();

    m_effects->reset();

    m_presentation->dispatchMap();

    m_realSize->setCallbackOnBegin(
        [this](auto) {
            if (!m_isMapped || m_backend->traits().overrideRedirect)
                return;

            g_pEventLoopManager->doLater([this, self = m_self] {
                if (!self)
                    return;

                sendWindowSize();
            });
        },
        false);

    m_realSize->setUpdateCallback([this](auto) {
        if (!m_isMapped)
            return;

        m_presentation->updateDecorations();

        m_events.resize.emit();
    });

    m_realPosition->setUpdateCallback([this](auto) {
        if (!m_isMapped)
            return;

        m_presentation->updateDecorations();

        if (m_monitor != m_prevMonitor) {
            m_prevMonitor = m_monitor;
            m_events.monitorChanged.emit();
        }
    });

    m_presentation->alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(1.F);

    m_presentation->setAnimatingIn(true);

    if (m_backend->isX11())
        return;

    setSubsurfaceHead(CSubsurface::create(m_self.lock()));
    setPopupHead(CPopup::create(m_self.lock()));
}

void CWindow::setHidden(bool hidden) {
    m_hidden = hidden;

    if (hidden)
        m_events.hide.emit();

    if (hidden && Desktop::focusState()->window() == m_self)
        Desktop::focusState()->window().reset();

    setSuspended(hidden);
}

bool CWindow::isHidden() const {
    return m_hidden;
}

void CWindow::onInputBlockStateUpdated(bool blocked) {
    if (blocked && Desktop::focusState()->window() == m_self)
        Desktop::focusState()->fullWindowFocus(nullptr, eFocusReason::FOCUS_REASON_SWITCH_TO_WINDOW_SOFT);
}

bool CWindow::isAllowedOverFullscreen() const {

    if (!m_workspace)
        return false;

    const auto FSWINDOW = Fullscreen::controller()->getFullscreenWindow(m_workspace);
    return m_fullscreenPolicy->effectiveAllowedOverFullscreen({
        .isFullscreenWindow    = m_self == Fullscreen::controller()->getFullscreenWindow(m_workspace, true),
        .pinned                = sc<bool>(m_state & WINDOW_STATE_PINNED),
        .groupedWithFullscreen = FSWINDOW && FSWINDOW->grouping().group() && FSWINDOW->grouping().group()->has(m_self.lock()),
    });
}

bool CWindow::isBlockedByFullscreen() const {
    if (!m_workspace || !Fullscreen::controller()->hasFullscreen(m_workspace))
        return false;

    return !isAllowedOverFullscreen();
}

bool CWindow::isFadingOutUnderFullscreen() const {
    return isBlockedByFullscreen() && m_presentation->alpha(WINDOW_ALPHA_FULLSCREEN)->isBeingAnimated() && m_presentation->alphaValue(WINDOW_ALPHA_FULLSCREEN) > 0.F;
}

bool CWindow::shouldRenderOverFullscreen() const {
    return isAllowedOverFullscreen() || !isFadingOutUnderFullscreen();
}

void CWindow::updateFullscreenInputState() {
    setInputBlocked(FOCUS_BLOCK_BELOW_FULLSCREEN, isBlockedByFullscreen());
}

Vector2D CWindow::middle() {
    return m_realPosition->goal() + m_realSize->goal() / 2.f;
}

void CWindow::updateWindowData() {
    const auto PWORKSPACE    = m_workspace;
    const auto WORKSPACERULE = PWORKSPACE ? Config::workspaceRuleMgr()->getWorkspaceRuleFor(PWORKSPACE) : std::nullopt;
    updateWindowData(WORKSPACERULE.value_or(Config::CWorkspaceRule{}));
}

void CWindow::updateWindowData(const Config::CWorkspaceRule& workspaceRule) {
    if (workspaceRule.m_noBorder.value_or(false))
        m_ruleApplicator->borderSize().matchOptional(std::optional<Config::INTEGER>(0), Desktop::Types::PRIORITY_WORKSPACE_RULE);
    else if (workspaceRule.m_borderSize)
        m_ruleApplicator->borderSize().matchOptional(workspaceRule.m_borderSize, Desktop::Types::PRIORITY_WORKSPACE_RULE);
    else
        m_ruleApplicator->borderSize().matchOptional(std::nullopt, Desktop::Types::PRIORITY_WORKSPACE_RULE);
    m_ruleApplicator->decorate().matchOptional(workspaceRule.m_decorate, Desktop::Types::PRIORITY_WORKSPACE_RULE);
    m_ruleApplicator->rounding().matchOptional(workspaceRule.m_noRounding.value_or(false) ? std::optional<Config::INTEGER>(0) : std::nullopt,
                                               Desktop::Types::PRIORITY_WORKSPACE_RULE);
    m_ruleApplicator->noShadow().matchOptional(workspaceRule.m_noShadow, Desktop::Types::PRIORITY_WORKSPACE_RULE);

    m_presentation->invalidateBorderSize();
}

float CWindow::getScrollMouse() {
    static auto PINPUTSCROLLFACTOR = CConfigValue<Config::FLOAT>("input:scroll_factor");
    return m_ruleApplicator->scrollMouse().valueOr(*PINPUTSCROLLFACTOR);
}

float CWindow::getScrollTouchpad() {
    static auto PTOUCHPADSCROLLFACTOR = CConfigValue<Config::FLOAT>("input:touchpad:scroll_factor");
    return m_ruleApplicator->scrollTouchpad().valueOr(*PTOUCHPADSCROLLFACTOR);
}

bool CWindow::isScrollMouseOverridden() {
    return m_ruleApplicator->scrollMouse().hasValue();
}

bool CWindow::isScrollTouchpadOverridden() {
    return m_ruleApplicator->scrollTouchpad().hasValue();
}

bool CWindow::canBeTorn() {
    static auto PTEARING = CConfigValue<Config::INTEGER>("general:allow_tearing");
    return m_ruleApplicator->tearing().valueOr(sc<bool>(m_hints & WINDOW_HINT_TEAR)) && *PTEARING;
}

void CWindow::setSuspended(bool suspend) {
    if (suspend == m_suspended)
        return;

    if (m_backend->setSuspended(suspend))
        m_suspended = suspend;
}

bool CWindow::clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize) {
    return m_target->clampWindowSize(minSize, maxSize);
}

WORKSPACEID CWindow::workspaceID() {
    return m_workspace ? m_workspace->m_id : m_lastWorkspace;
}

MONITORID CWindow::monitorID() {
    return m_monitor ? m_monitor->m_id : MONITOR_INVALID;
}

bool CWindow::onSpecialWorkspace() {
    return m_workspace ? m_workspace->m_isSpecialWorkspace : State::workspaceState()->isSpecial(m_lastWorkspace);
}

std::unordered_map<std::string, std::string> CWindow::getEnv() {

    const auto PID = m_backend->pid();

    if (PID <= 1)
        return {};

    std::unordered_map<std::string, std::string> results;

    std::vector<char>                            buffer;
    size_t                                       needle = 0;

#if defined(__linux__)
    //
    std::string   environFile = std::format("/proc/{}/environ", PID);
    std::ifstream ifs(environFile, std::ios::binary);

    if (!ifs.good())
        return {};

    buffer.resize(512, '\0');
    while (ifs.read(buffer.data() + needle, 512)) {
        buffer.resize(buffer.size() + 512, '\0');
        needle += 512;
    }
    needle += ifs.gcount();
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    int    mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ENV, sc<int>(PID)};
    size_t len    = 0;

    if (sysctl(mib, 4, nullptr, &len, nullptr, 0) < 0 || len == 0)
        return {};

    buffer.resize(len, '\0');

    if (sysctl(mib, 4, buffer.data(), &len, nullptr, 0) < 0)
        return {};

    needle = len;
#endif

    if (needle <= 1)
        return {};

    buffer.resize(needle + 1, '\0');
    std::replace(buffer.begin(), buffer.end() - 1, '\0', '\n');

    CVarList envs(std::string{buffer.data(), buffer.size() - 1}, 0, '\n', true);

    for (auto const& e : envs) {
        if (!e.contains('='))
            continue;

        const auto EQ            = e.find_first_of('=');
        results[e.substr(0, EQ)] = e.substr(EQ + 1);
    }

    return results;
}

void CWindow::activate(bool force) {
    if (Desktop::focusState()->window() == m_self)
        return;

    static auto PFOCUSONACTIVATE = CConfigValue<Config::INTEGER>("misc:focus_on_activate");

    m_hints |= WINDOW_HINT_URGENT;

    IPC::Socket2::sock()->postEvent({.event = "urgent", .data = std::format("{:x}", rc<uintptr_t>(this))});
    Event::bus()->m_events.window.urgent.emit(m_self.lock());

    if (!force && (!m_ruleApplicator->focusOnActivate().valueOr(*PFOCUSONACTIVATE) || m_requestSuppression.activateFocusOnly || m_requestSuppression.activate))
        return;

    if (!m_isMapped) {
        Log::logger->log(Log::DEBUG, "Ignoring CWindow::activate focus/warp, window is not mapped yet.");
        return;
    }

    if (m_target->floating())
        Desktop::windowState()->raise(m_self.lock());

    Desktop::focusState()->fullWindowFocus(m_self.lock(), FOCUS_REASON_DESKTOP_STATE_CHANGE);
    warpCursor();
}

void CWindow::onUpdateState(const SBackendStateRequest& request) {
    requestClientFullscreen({
        .fullscreen        = request.fullscreen,
        .maximized         = request.maximized,
        .fullscreenMonitor = request.fullscreenMonitor,
        .origin            = SClientFullscreenRequest::ORIGIN_BACKEND,
    });
}

void CWindow::requestClientFullscreen(const SClientFullscreenRequest& request) {
    // a client re-asserting fullscreen every frame (notably gamescope) would snap the window back the instant
    // the drag handler pulls it out of fullscreen, so ignore the request while this window is being dragged.
    const auto  DRAGTARGET         = g_layoutManager->dragController()->target();
    const bool  DRAGGINGTHISWINDOW = DRAGTARGET && DRAGTARGET->window() == m_self.lock();
    const bool  BACKEND_REQUEST    = request.origin == SClientFullscreenRequest::ORIGIN_BACKEND;
    const auto& SUPPRESSION        = m_fullscreenPolicy->requestSuppression();

    if (request.fullscreen.has_value() && !SUPPRESSION.fullscreen && (!BACKEND_REQUEST || !DRAGGINGTHISWINDOW)) {
        std::optional<MONITORID> requestedMonitor;
        if (request.fullscreenMonitor.has_value() && request.fullscreenMonitor.value() != MONITOR_INVALID && !SUPPRESSION.fullscreenOutput) {
            const auto monitor = State::monitorState()->query().id(request.fullscreenMonitor.value()).run();
            if (monitor) {
                requestedMonitor = monitor->m_id;
                if (m_isMapped) {
                    Desktop::globalWindowController()->moveWindowToWorkspace(m_self.lock(), monitor->m_activeWorkspace);
                    Desktop::focusState()->rawMonitorFocus(monitor);
                }
            }
        }

        if (m_isMapped) {
            if (request.fullscreen.value())
                Fullscreen::controller()->setFullscreenMode(m_self.lock(), std::nullopt, Fullscreen::FSMODE_FULLSCREEN);
            else {
                // If window's fullscreen, un-fullscreen it. if it's not FS, let it keep its current FS mode
                if (Fullscreen::controller()->getFullscreenModes(m_self.lock()).client == Fullscreen::FSMODE_FULLSCREEN)
                    Fullscreen::controller()->setFullscreenMode(m_self.lock(), std::nullopt, Fullscreen::FSMODE_NONE);
            }
        } else if (request.fullscreen.value())
            m_fullscreenPolicy->setPendingClientRequest(Fullscreen::FSMODE_FULLSCREEN, requestedMonitor);
        else
            m_fullscreenPolicy->clearPendingClientMode(Fullscreen::FSMODE_FULLSCREEN);
    }

    if (request.maximized.has_value() && !SUPPRESSION.maximize) {
        if (m_isMapped) {
            if (!BACKEND_REQUEST || !m_fullscreenPolicy->consumeExpectedMaximizeEcho(request.maximized.value())) {
                const auto WINDOW       = m_self.lock();
                const auto CLIENT_STATE = Fullscreen::controller()->getFullscreenModes(WINDOW).client;
                if (request.maximized.value())
                    Fullscreen::controller()->setFullscreenMode(WINDOW, std::nullopt, Fullscreen::FSMODE_MAXIMIZED);
                else if (CLIENT_STATE == Fullscreen::FSMODE_MAXIMIZED)
                    Fullscreen::controller()->setFullscreenMode(WINDOW, std::nullopt, Fullscreen::FSMODE_NONE);
            }
        } else if (request.maximized.value())
            m_fullscreenPolicy->setPendingClientRequest(Fullscreen::FSMODE_MAXIMIZED);
        else
            m_fullscreenPolicy->clearPendingClientMode(Fullscreen::FSMODE_MAXIMIZED);
    }
}

void CWindow::onUpdateMeta(const SBackendMetadata& metadata) {
    const auto& NEWTITLE = metadata.title;
    bool        doUpdate = false;

    if (m_metadata->updateTitle(NEWTITLE)) {
        IPC::Socket2::sock()->postEvent({.event = "windowtitle", .data = std::format("{:x}", rc<uintptr_t>(this))});
        IPC::Socket2::sock()->postEvent({.event = "windowtitlev2", .data = std::format("{:x},{}", rc<uintptr_t>(this), m_metadata->title())});
        Event::bus()->m_events.window.title.emit(m_self.lock());

        if (m_self == Desktop::focusState()->window()) { // if it's the active, let's post an event to update others
            IPC::Socket2::sock()->postEvent({.event = "activewindow", .data = std::format("{},{}", m_metadata->appID(), m_metadata->title())});
            IPC::Socket2::sock()->postEvent({.event = "activewindowv2", .data = std::format("{:x}", rc<uintptr_t>(this))});

            // no need for a hook event
        }

        Log::logger->log(Log::DEBUG, "Window {:x} set title to {}", rc<uintptr_t>(this), m_metadata->title());
        doUpdate = true;
    }

    const auto& NEWCLASS = metadata.appID;
    if (m_metadata->updateAppID(NEWCLASS)) {
        Event::bus()->m_events.window.class_.emit(m_self.lock());

        if (m_self == Desktop::focusState()->window()) { // if it's the active, let's post an event to update others
            IPC::Socket2::sock()->postEvent({.event = "activewindow", .data = std::format("{},{}", m_metadata->appID(), m_metadata->title())});
            IPC::Socket2::sock()->postEvent({.event = "activewindowv2", .data = std::format("{:x}", rc<uintptr_t>(this))});

            // no need for a hook event
        }

        Log::logger->log(Log::DEBUG, "Window {:x} set class to {}", rc<uintptr_t>(this), m_metadata->appID());
        doUpdate = true;
    }

    if (doUpdate) {
        m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_TITLE | Desktop::Rule::RULE_PROP_CLASS);
        updateToplevel();
    }
}

void CWindow::onSurfaceChanged(SP<CWLSurfaceResource> surface) {
    if (surface && !m_wlSurface->resource())
        m_wlSurface->assign(surface, m_self.lock());
    else if (!surface && m_wlSurface->resource())
        m_wlSurface->unassign();

    Log::logger->log(Log::DEBUG, "window client {} -> association to {:x}", m_backend->clientID().id, rc<uintptr_t>(m_wlSurface->resource().get()));
}

void CWindow::onConfigureRequest(const CBox& box) {
    if (!m_backend->isX11())
        return;

    if (!m_backend->surface() || !m_backend->isMapped() || !m_isMapped) {
        m_backend->configure(box, m_monitor.lock(), true);
        acknowledgeClientGeometry(box);
        return;
    }

    g_pHyprRenderer->damageWindow(m_self.lock());

    if (!m_target->floating() || Fullscreen::controller()->isFullscreen(m_self.lock()) || g_layoutManager->dragController()->target() == layoutTarget() ||
        m_requestSuppression.x11ConfigureRequest) {
        sendWindowSize(true);
        g_pInputManager->refocus();
        g_pHyprRenderer->damageWindow(m_self.lock());
        return;
    }

    if (box.size() > Vector2D{1, 1})
        setHidden(false);
    else
        setHidden(true);

    layoutTarget()->setPositionGlobal(box);
    m_backend->configure(box, m_monitor.lock());
    acknowledgeClientGeometry(box);
    m_presentation->updateDecorations();

    if (!m_workspace || !m_workspace->isVisible())
        return; // further things are only for visible windows

    const auto monitorByRequestedPosition = State::monitorState()->query().vec(m_realPosition->goal() + m_realSize->goal() / 2.f).run();
    const auto currentMonitor             = m_workspace->m_monitor.lock();

    Log::logger->log(
        Log::DEBUG,
        "onX11ConfigureRequest: window '{}' ({:#x}) - workspace '{}' (special={}), currentMonitor='{}', monitorByRequestedPosition='{}', pos={:.0f},{:.0f}, size={:.0f},{:.0f}",
        m_metadata->title(), (uintptr_t)this, m_workspace->m_name, m_workspace->m_isSpecialWorkspace, currentMonitor ? currentMonitor->m_name : "null",
        monitorByRequestedPosition ? monitorByRequestedPosition->m_name : "null", m_realPosition->goal().x, m_realPosition->goal().y, m_realSize->goal().x, m_realSize->goal().y);

    // Reassign workspace only when moving to a different monitor and not on a special workspace
    // X11 apps send configure requests with positions based on XWayland's monitor layout, such as "0,0",
    // which would incorrectly move windows off special workspaces
    if (monitorByRequestedPosition && monitorByRequestedPosition != currentMonitor && !m_workspace->m_isSpecialWorkspace) {
        Log::logger->log(Log::DEBUG, "onX11ConfigureRequest: reassigning workspace from '{}' to '{}'", m_workspace->m_name, monitorByRequestedPosition->m_activeWorkspace->m_name);
        m_workspace = monitorByRequestedPosition->m_activeWorkspace;
    }

    Desktop::windowState()->raise(m_self.lock());

    m_fullscreenPolicy->setAllowedOverFullscreen(true);

    g_pHyprRenderer->damageWindow(m_self.lock());
}

void CWindow::warpCursor(bool force) {
    static auto PERSISTENTWARPS        = CConfigValue<Config::INTEGER>("cursor:persistent_warps");
    const auto  coords                 = m_relativeCursorCoordsOnLastWarp;
    m_relativeCursorCoordsOnLastWarp.x = -1; // reset m_vRelativeCursorCoordsOnLastWarp

    const auto BOX = layoutBox();

    if (*PERSISTENTWARPS && coords.x > 0 && coords.y > 0 && coords < BOX.size()) // don't warp cursor outside the window
        Pointer::pointerController()->warpTo(BOX.pos() + coords, force);
    else
        Pointer::pointerController()->warpTo(middle(), force);
}

bool CWindow::shouldntFocus() const {
    const auto TRAITS = m_backend->traits();
    return !m_ruleApplicator->allowsInput().valueOrDefault() && (TRAITS.preventsFocus || (TRAITS.overrideRedirect && !TRAITS.wantsFocus));
}

bool CWindow::suggestsFloat(bool pending) const {
    if (m_backend->traits().suggestsFloat)
        return true;

    if (m_backend->isX11())
        return false;

    const auto HINTS = m_backend->geometryHints(pending ? eBackendState::BACKEND_STATE_PENDING : eBackendState::BACKEND_STATE_CURRENT);
    if (!HINTS.minSize || !HINTS.maxSize || HINTS.minSize->x <= 1 || HINTS.minSize->y <= 1)
        return false;

    return HINTS.minSize->x == HINTS.maxSize->x || HINTS.minSize->y == HINTS.maxSize->y;
}

void CWindow::acknowledgeClientGeometry(const CBox& logicalBox) {
    m_backend->acknowledgeConfigure(m_backend->logicalToClient(logicalBox, m_monitor.lock()));
}

void CWindow::sendWindowSize(bool force) {
    m_target->sendWindowSize(force);
}

NContentType::eContentType CWindow::getContentType() {
    if (!m_wlSurface || !m_wlSurface->resource() || !m_wlSurface->resource()->m_contentType.valid())
        return CONTENT_TYPE_NONE;

    return m_wlSurface->resource()->m_contentType->m_value;
}

void CWindow::setContentType(NContentType::eContentType contentType) {
    if (!m_wlSurface->resource()->m_contentType.valid())
        m_wlSurface->resource()->m_contentType = PROTO::contentType->getContentType(m_wlSurface->resource());
    // else disallow content type change if proto is used?

    Log::logger->log(Log::INFO, "ContentType for window {}", sc<int>(contentType));
    m_wlSurface->resource()->m_contentType->m_value = contentType;
}

void CWindow::deactivateGroupMembers() {
    if (!m_grouping->group())
        return;
    for (const auto& w : m_grouping->group()->windows()) {
        if (w != m_self.lock()) {
            // we don't want to deactivate unfocused xwayland windows
            // because X is weird, keep the behavior for wayland windows
            // also its not really needed for xwayland windows
            // ref: #9760 #9294
            if (!w->backend().isX11())
                w->backend().setActive(false);
        }
    }
}

bool CWindow::isNotResponding() {
    return g_pANRManager->isNotResponding(m_self.lock());
}

bool CWindow::priorityFocus() {
    return !m_backend->isX11() && CAsyncDialogBox::isPriorityDialogBox(m_backend->pid());
}

SP<CWLSurfaceResource> CWindow::getSolitaryResource() {
    if (!m_wlSurface || !m_wlSurface->resource())
        return nullptr;

    auto res = m_wlSurface->resource();
    if (m_backend->isX11())
        return res;

    if (popupTreeSize())
        return nullptr;

    if (res->m_subsurfaces.size() == 0)
        return res;

    if (res->m_subsurfaces.size() >= 1) {
        if (!res->hasVisibleSubsurface())
            return res;

        if (res->m_subsurfaces.size() == 1) {
            if (res->m_subsurfaces[0].expired() || res->m_subsurfaces[0]->m_surface.expired())
                return nullptr;
            auto surf = res->m_subsurfaces[0]->m_surface.lock();
            if (!surf || surf->m_subsurfaces.size() != 0 || surf->extends() != res->extends() || !surf->m_current.texture || !surf->m_current.texture->m_opaque)
                return nullptr;
            return surf;
        }
    }

    return nullptr;
}

std::optional<Vector2D> CWindow::calculateExpression(const Math::SExpressionVec2& expr) {
    return m_target->calculateExpression(expr);
}

static void setVector2DAnimToMove(WP<CBaseAnimatedVariable> pav) {
    if (!pav)
        return;

    CAnimatedVariable<Vector2D>* animvar = dc<CAnimatedVariable<Vector2D>*>(pav.get());
    animvar->setConfig(Config::animationTree()->getAnimationPropertyConfig("windowsMove"));

    if (animvar->m_Context.pWindow) {
        animvar->m_Context.pWindow->presentation().setAnimatingIn(false);

        if (!animvar->m_Context.pWindow->positionAnimation()->isBeingAnimated() && !animvar->m_Context.pWindow->sizeAnimation()->isBeingAnimated()) {
            animvar->m_Context.pWindow->effects().reset();
        }
    }
}

void CWindow::mapWindow() {
    static auto PINACTIVEALPHA     = CConfigValue<Config::FLOAT>("decoration:inactive_opacity");
    static auto PACTIVEALPHA       = CConfigValue<Config::FLOAT>("decoration:active_opacity");
    static auto PDIMSTRENGTH       = CConfigValue<Config::FLOAT>("decoration:dim_strength");
    static auto PNEWTAKESOVERFS    = CConfigValue<Config::INTEGER>("misc:on_focus_under_fullscreen");
    static auto PINITIALWSTRACKING = CConfigValue<Config::INTEGER>("misc:initial_workspace_tracking");
    static auto PAUTOGROUP         = CConfigValue<Config::INTEGER>("group:auto_group");

    auto        PMONITOR = Desktop::focusState()->monitor();
    if (!Desktop::focusState()->monitor()) {
        Desktop::focusState()->rawMonitorFocus(State::monitorState()->query().vec({}).run());
        PMONITOR = Desktop::focusState()->monitor();
    }
    if (!PMONITOR || (!PMONITOR->m_activeSpecialWorkspace && !PMONITOR->m_activeWorkspace)) {
        Log::logger->log(Log::ERR, "mapWindow: no valid monitor/workspace, aborting map for {:x}", (uintptr_t)this);
        return;
    }
    auto PWORKSPACE = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
    m_monitor       = PMONITOR;
    m_workspace     = PWORKSPACE;
    m_isMapped      = true;
    m_presentation->setAnimatingIn(true);
    const auto METADATA = m_backend->metadata();
    const auto TRAITS   = m_backend->traits();
    m_metadata->initializeOnFirstMap(METADATA.title, METADATA.appID);
    m_state |= WINDOW_STATE_FIRST_MAP;

    // check for token
    std::string requestedWorkspace = "";
    bool        workspaceSilent    = false;

    bool        monitorSilent = false;

    if (*PINITIALWSTRACKING) {
        const auto WINDOWENV = getEnv();
        if (WINDOWENV.contains("HL_INITIAL_WORKSPACE_TOKEN")) {
            const auto SZTOKEN = WINDOWENV.at("HL_INITIAL_WORKSPACE_TOKEN");
            Log::logger->log(Log::DEBUG, "New window contains HL_INITIAL_WORKSPACE_TOKEN: {}", SZTOKEN);
            const auto TOKEN = g_pTokenManager->getToken(SZTOKEN);
            if (TOKEN) {
                // find workspace and use it
                Desktop::View::SInitialWorkspaceToken WS = std::any_cast<Desktop::View::SInitialWorkspaceToken>(TOKEN->m_data);

                Log::logger->log(Log::DEBUG, "HL_INITIAL_WORKSPACE_TOKEN {} -> {}", SZTOKEN, WS.workspace);

                if (State::workspaceState()->query().string(WS.workspace).run() != m_workspace) {
                    requestedWorkspace = WS.workspace;
                    workspaceSilent    = true;
                }

                if (*PINITIALWSTRACKING == 1) // one-shot token
                    g_pTokenManager->removeToken(TOKEN);
                else if (*PINITIALWSTRACKING == 2) { // persistent
                    if (WS.primaryOwner.expired()) {
                        WS.primaryOwner = m_self.lock();
                        TOKEN->m_data   = WS;
                    }

                    m_initialWorkspaceToken = SZTOKEN;
                }
            }
        }
    }

    if (g_pInputManager->m_lastFocusOnLS) // waybar fix
        g_pInputManager->releaseAllMouseButtons();

    // registers the animated vars and stuff
    onMap();

    if (suggestsFloat()) {
        m_target->setFloatingInitial(true);
    }

    if (TRAITS.suggestsNoInitialFocus)
        m_state |= WINDOW_STATE_NO_INITIAL_FOCUS;

    // window rules
    std::optional<Fullscreen::eFullscreenMode> requestedInternalFSMode, requestedClientFSMode;
    std::optional<Fullscreen::SFullscreenMode> requestedFSState;
    const auto&                                PENDING_CLIENT_FS = m_fullscreenPolicy->pendingClientRequest();
    requestedClientFSMode                                        = PENDING_CLIENT_FS.mode;
    if (!requestedClientFSMode.has_value() && m_backend->isX11() && TRAITS.fullscreen)
        requestedClientFSMode = Fullscreen::FSMODE_FULLSCREEN;
    MONITORID requestedFSMonitor = PENDING_CLIENT_FS.monitor.value_or(MONITOR_INVALID);

    auto      setStaticProps = [&]() {
        SFullscreenRequestSuppression fullscreenSuppression;
        m_requestSuppression = {};

        if (!m_ruleApplicator->static_.monitor.empty()) {
            const auto& MONITORSTR = m_ruleApplicator->static_.monitor;
            if (MONITORSTR == "unset")
                m_monitor = PMONITOR;
            else {
                const auto ARGPOS = MONITORSTR.find_last_of(' ');
                monitorSilent     = ARGPOS != std::string::npos && MONITORSTR.substr(ARGPOS).contains("silent");
                const auto MONITOR =
                    State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).configString(monitorSilent ? MONITORSTR.substr(0, ARGPOS) : MONITORSTR).run();

                if (MONITOR) {
                    m_monitor = MONITOR;

                    const auto PMONITORFROMID = m_monitor.lock();

                    if (m_monitor != PMONITOR && !monitorSilent) // NOLINTNEXTLINE
                        Config::Actions::focusMonitor(PMONITORFROMID);

                    PMONITOR = PMONITORFROMID;

                    m_workspace = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
                    PWORKSPACE  = m_workspace;

                    Log::logger->log(Log::DEBUG, "Rule monitor, applying to {:mw}", m_self.lock());
                    requestedFSMonitor = MONITOR_INVALID;
                } else
                    Log::logger->log(Log::ERR, "No monitor in monitor {} rule", MONITORSTR);
            }
        }

        if (!m_ruleApplicator->static_.workspace.empty()) {
            const auto WORKSPACERQ = m_ruleApplicator->static_.workspace;

            if (WORKSPACERQ == "unset")
                requestedWorkspace = "";
            else
                requestedWorkspace = WORKSPACERQ;

            const auto JUSTWORKSPACE = WORKSPACERQ.contains(' ') ? WORKSPACERQ.substr(0, WORKSPACERQ.find_first_of(' ')) : WORKSPACERQ;

            if (JUSTWORKSPACE == PWORKSPACE->m_name || JUSTWORKSPACE == std::format("name:{}", PWORKSPACE->m_name))
                requestedWorkspace = "";

            Log::logger->log(Log::DEBUG, "Rule workspace matched by {}, {} applied.", m_self.lock(), m_ruleApplicator->static_.workspace);
            requestedFSMonitor = MONITOR_INVALID;
        }

        m_target->setFloatingInitial(m_ruleApplicator->static_.floating.value_or(m_target->floating()));
        m_target->setPseudo(m_ruleApplicator->static_.pseudo.value_or(m_target->isPseudo()));
        if (m_ruleApplicator->static_.noInitialFocus.value_or(sc<bool>(m_state & WINDOW_STATE_NO_INITIAL_FOCUS)))
            m_state |= WINDOW_STATE_NO_INITIAL_FOCUS;
        else
            m_state &= ~WINDOW_STATE_NO_INITIAL_FOCUS;

        if (m_ruleApplicator->static_.pin.value_or(sc<bool>(m_state & WINDOW_STATE_PINNED)))
            m_state |= WINDOW_STATE_PINNED;
        else
            m_state &= ~WINDOW_STATE_PINNED;

        if (m_ruleApplicator->static_.fullscreenStateClient || m_ruleApplicator->static_.fullscreenStateInternal) {
            requestedFSState = Fullscreen::SFullscreenMode{
                .internal = sc<Fullscreen::eFullscreenMode>(m_ruleApplicator->static_.fullscreenStateInternal.value_or(0)),
                .client   = sc<Fullscreen::eFullscreenMode>(m_ruleApplicator->static_.fullscreenStateClient.value_or(0)),
            };
        }

        for (const auto& var : m_ruleApplicator->static_.suppressEvent) {
            if (var == "fullscreen")
                fullscreenSuppression.fullscreen = true;
            else if (var == "maximize")
                fullscreenSuppression.maximize = true;
            else if (var == "activate")
                m_requestSuppression.activate = true;
            else if (var == "activatefocus")
                m_requestSuppression.activateFocusOnly = true;
            else if (var == "fullscreenoutput")
                fullscreenSuppression.fullscreenOutput = true;
            else if (var == "x11configurerequest")
                m_requestSuppression.x11ConfigureRequest = true;
            else
                Log::logger->log(Log::ERR, "Error while parsing suppressevent windowrule: unknown event type {}", var);
        }
        m_fullscreenPolicy->setRequestSuppression(fullscreenSuppression);

        if (m_ruleApplicator->static_.fullscreen.value_or(false))
            requestedInternalFSMode = Fullscreen::FSMODE_FULLSCREEN;

        if (m_ruleApplicator->static_.maximize.value_or(false))
            requestedInternalFSMode = Fullscreen::FSMODE_MAXIMIZED;

        if (!m_ruleApplicator->static_.group.empty())
            m_grouping->applyRule(m_ruleApplicator->static_.group);

        if (m_ruleApplicator->static_.content)
            setContentType(sc<NContentType::eContentType>(m_ruleApplicator->static_.content.value()));

        if (m_ruleApplicator->static_.noCloseFor)
            m_closeableSince = Time::steadyNow() + std::chrono::milliseconds(m_ruleApplicator->static_.noCloseFor.value());
    };

    const bool recheck = m_ruleApplicator->readStaticRules();
    setStaticProps();
    if (recheck) {
        m_ruleApplicator->recheckStaticRules();
        setStaticProps();
    }

    // make it uncloseable if it's a Hyprland dialog
    // TODO: make some closeable?
    if (CAsyncDialogBox::isAsyncDialogBox(m_backend->pid()))
        m_closeableSince = Time::steadyNow() + std::chrono::years(10 /* Should be enough, no? */);

    // disallow tiled pinned
    if ((m_state & WINDOW_STATE_PINNED) && !m_target->floating())
        m_state &= ~WINDOW_STATE_PINNED;

    CVarList2 WORKSPACEARGS = CVarList2(std::move(requestedWorkspace), 0, ' ', false, false);

    if (!WORKSPACEARGS[0].empty()) {
        WORKSPACEID requestedWorkspaceID;
        std::string requestedWorkspaceName;
        if (WORKSPACEARGS.contains("silent"))
            workspaceSilent = true;

        auto joined = WORKSPACEARGS.join(" ", 0, workspaceSilent ? WORKSPACEARGS.size() - 1 : 0);
        if (joined.starts_with("empty") && PWORKSPACE->getWindowCount() == 0) {
            requestedWorkspaceID   = PWORKSPACE->m_id;
            requestedWorkspaceName = PWORKSPACE->m_name;
        } else {
            auto result            = getWorkspaceIDNameFromString(joined);
            requestedWorkspaceID   = result.id;
            requestedWorkspaceName = result.name;
        }

        if (requestedWorkspaceID != WORKSPACE_INVALID) {
            auto pWorkspace = State::workspaceState()->query().id(requestedWorkspaceID).run();

            if (!pWorkspace)
                pWorkspace = State::workspaceState()->create(requestedWorkspaceID, monitorID(), requestedWorkspaceName, false);

            PWORKSPACE = pWorkspace;

            m_workspace = pWorkspace;
            m_monitor   = pWorkspace->m_monitor;

            if (m_monitor && m_monitor->m_activeSpecialWorkspace && !pWorkspace->m_isSpecialWorkspace)
                workspaceSilent = true;

            if (!workspaceSilent) {
                if (pWorkspace->m_isSpecialWorkspace && pWorkspace->m_monitor)
                    pWorkspace->m_monitor->setSpecialWorkspace(pWorkspace);
                else if (PMONITOR->activeWorkspaceID() != requestedWorkspaceID && !(m_state & WINDOW_STATE_NO_INITIAL_FOCUS)) // NOLINTNEXTLINE
                    Config::Actions::changeWorkspace(requestedWorkspaceName);

                PMONITOR = Desktop::focusState()->monitor();
            }

            requestedFSMonitor = MONITOR_INVALID;
        } else
            workspaceSilent = false;
    }

    if (m_fullscreenPolicy->requestSuppression().fullscreenOutput)
        requestedFSMonitor = MONITOR_INVALID;
    else if (requestedFSMonitor != MONITOR_INVALID) {
        if (const auto PM = State::monitorState()->query().id(requestedFSMonitor).run(); PM)
            m_monitor = PM;

        const auto PMONITORFROMID = m_monitor.lock();

        if (m_monitor != PMONITOR) { // NOLINTNEXTLINE
            Config::Actions::focusMonitor(PMONITORFROMID);
            PMONITOR = PMONITORFROMID;
        }
        m_workspace = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
        PWORKSPACE  = m_workspace;

        Log::logger->log(Log::DEBUG, "Requested monitor, applying to {:mw}", m_self.lock());
    }

    PMONITOR = m_monitor.lock();

    m_swallowing->reserveCandidate();

    // emit the IPC event before the layout might focus the window to avoid a focus event first
    IPC::Socket2::sock()->postEvent({"openwindow", std::format("{:x},{},{},{}", m_self.lock(), PWORKSPACE->m_name, m_metadata->appID(), m_metadata->title())});
    Event::bus()->m_events.window.openEarly.emit(m_self.lock());

    if (m_swallowing->activate()) {
        ;
    } else if (*PAUTOGROUP                                                                          // auto_group enabled
               && Desktop::focusState()->window()                                                   // focused window exists
               && m_grouping->canBeGroupedInto(Desktop::focusState()->window()->grouping().group()) // we can group
               && Desktop::focusState()->window()->m_workspace == m_workspace                       // workspaces match, we're not opening on another ws
               && !suggestsFloat() && !TRAITS.overrideRedirect                                      // not a window that should float or X11
               && !(m_target->floating() && !Desktop::focusState()->window()->isFloating())         // do not auto-group a floated window into a tiled group
               && !TRAITS.modal                                                                     // no modal grouping
    ) {
        // add to group if we are focused on one
        Desktop::focusState()->window()->grouping().group()->add(m_self.lock());
    } else
        g_layoutManager->newTarget(m_target, m_workspace->m_space);

    if (!m_grouping->group() && (m_grouping->rules() & GROUP_SET))
        CGroup::create({m_self});

    updateWindowData();

    if (m_target->floating()) {
        // all new floating windows are allowed over existing FS windows.
        m_fullscreenPolicy->setAllowedOverFullscreen(true);

        // set the pseudo size to the GOAL of our current size
        // because the windows are animated on RealSize
        m_target->setPseudoSize(m_realSize->goal());

        Desktop::windowState()->raise(m_self.lock());
    } else {
        bool setPseudo = false;

        if (m_ruleApplicator->static_.size) {
            const auto COMPUTED = calculateExpression(*m_ruleApplicator->static_.size);
            if (!COMPUTED)
                Log::logger->log(Log::ERR, "failed to parse {} as an expression", m_ruleApplicator->static_.size->toString());
            else {
                setPseudo = true;
                m_target->setPseudoSize(*COMPUTED);
                setHidden(false);
            }
        }

        if (!setPseudo)
            m_target->setPseudoSize(m_realSize->goal() - Vector2D(10, 10));
    }

    const auto PFOCUSEDWINDOWPREV = Desktop::focusState()->window();

    if (m_ruleApplicator->allowsInput().valueOrDefault()) { // if default value wasn't set to false getPriority() would throw an exception
        m_ruleApplicator->noFocusOverride(Desktop::Types::COverridableVar(false, m_ruleApplicator->allowsInput().getPriority()));
        m_state &= ~WINDOW_STATE_NO_INITIAL_FOCUS;
    }

    // check LS focus grab
    const auto PFORCEFOCUS  = Desktop::viewState()->query().forceFocus().runWindow();
    const auto PLSFROMFOCUS = Desktop::viewState()->query().type(VIEW_TYPE_LAYER_SURFACE).surface(Desktop::focusState()->surface()).runLayer();
    if (PLSFROMFOCUS && PLSFROMFOCUS->m_layerSurface->m_current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        m_state |= WINDOW_STATE_NO_INITIAL_FOCUS;

    // emit the hook event here after basic stuff has been initialized
    // It must emit before FS operations so that window has decorations data
    Event::bus()->m_events.window.open.emit(m_self.lock());

    if (Fullscreen::controller()->hasFullscreen(m_workspace) && !requestedInternalFSMode.has_value() && !requestedClientFSMode.has_value() && !m_target->floating()) {
        if (*PNEWTAKESOVERFS == 0)
            m_state |= WINDOW_STATE_NO_INITIAL_FOCUS;
        else if (*PNEWTAKESOVERFS == 1)
            requestedInternalFSMode = Fullscreen::controller()->getFullscreenModes(m_workspace).internal;
        else if (*PNEWTAKESOVERFS == 2)
            Fullscreen::controller()->setFullscreenMode(Fullscreen::controller()->getFullscreenWindow(m_workspace), Fullscreen::FSMODE_NONE, std::nullopt);
    }

    if (!m_ruleApplicator->noFocus().valueOrDefault() && !(m_state & WINDOW_STATE_NO_INITIAL_FOCUS) && (!TRAITS.overrideRedirect || TRAITS.wantsFocus) && !workspaceSilent &&
        !monitorSilent && (!PFORCEFOCUS || PFORCEFOCUS == m_self.lock()) && !g_pInputManager->isConstrained()) {

        // don't steal pointer focus with X11 when buttons are held (e.g., during drags)
        // if the incoming window is an OR
        if (!m_backend->isX11() || !g_pInputManager->hasHeldButtons() || !TRAITS.overrideRedirect)
            Desktop::focusState()->fullWindowFocus(m_self.lock(), FOCUS_REASON_NEW_WINDOW);

        m_presentation->alpha(WINDOW_ALPHA_ACTIVE)->setValueAndWarp(*PACTIVEALPHA);
        m_presentation->warpDimPercent(m_ruleApplicator->noDim().valueOrDefault() ? 0.F : *PDIMSTRENGTH);
    } else {
        m_presentation->alpha(WINDOW_ALPHA_ACTIVE)->setValueAndWarp(*PINACTIVEALPHA);
        m_presentation->warpDimPercent(0.F);
    }

    if (requestedClientFSMode == Fullscreen::FSMODE_FULLSCREEN && m_fullscreenPolicy->requestSuppression().fullscreen)
        requestedClientFSMode.reset();
    if (requestedClientFSMode == Fullscreen::FSMODE_MAXIMIZED && m_fullscreenPolicy->requestSuppression().maximize)
        requestedClientFSMode.reset();

    if (!(m_state & WINDOW_STATE_NO_INITIAL_FOCUS) && (requestedInternalFSMode.has_value() || requestedClientFSMode.has_value() || requestedFSState.has_value())) {
        // fix fullscreen on requested (basically do a switcheroo)
        std::optional<bool> wasFullscreenLayoutHandled = std::nullopt;
        if (Fullscreen::controller()->hasFullscreen(m_workspace)) {
            auto fsWindow              = Fullscreen::controller()->getFullscreenWindow(m_workspace);
            wasFullscreenLayoutHandled = Fullscreen::controller()->layoutManagedFS(fsWindow);
            Fullscreen::controller()->setFullscreenMode(fsWindow, Fullscreen::FSMODE_NONE);
        }
        m_realPosition->warp();
        m_realSize->warp();
        m_effects->reset();
        if (requestedFSState.has_value()) {
            m_ruleApplicator->syncFullscreenOverride(Desktop::Types::COverridableVar(false, Desktop::Types::PRIORITY_WINDOW_RULE));
            Fullscreen::controller()->setFullscreenMode(m_self.lock(), requestedFSState.value().internal, requestedFSState.value().client, wasFullscreenLayoutHandled);
        } else if (requestedInternalFSMode.has_value() && requestedClientFSMode.has_value() && !m_ruleApplicator->syncFullscreen().valueOrDefault())
            Fullscreen::controller()->setFullscreenMode(m_self.lock(), requestedInternalFSMode, requestedClientFSMode, wasFullscreenLayoutHandled);
        else if (requestedInternalFSMode.has_value() || requestedClientFSMode.has_value())
            Fullscreen::controller()->setFullscreenMode(m_self.lock(), requestedInternalFSMode, requestedClientFSMode, wasFullscreenLayoutHandled);
    }

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    updateSurfaceScaleTransformDetails(true);
    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_ALL);

    if (workspaceSilent) {
        if (validMapped(PFOCUSEDWINDOWPREV)) {
            Desktop::focusState()->rawWindowFocus(PFOCUSEDWINDOWPREV, FOCUS_REASON_NEW_WINDOW);
            PFOCUSEDWINDOWPREV->presentation().updateDecorations(); // need to for some reason i cba to find out why
        } else if (!PFOCUSEDWINDOWPREV)
            Desktop::focusState()->rawWindowFocus(nullptr, FOCUS_REASON_NEW_WINDOW);
    }

    m_state &= ~WINDOW_STATE_FIRST_MAP;
    m_fullscreenPolicy->consumePendingClientRequest();

    Log::logger->log(Log::DEBUG, "Map request dispatched, monitor {}, window pos: {:5j}, window size: {:5j}", PMONITOR->m_name, m_realPosition->goal(), m_realSize->goal());

    // apply data from default decos. Borders, shadows.
    g_pDecorationPositioner->forceRecalcFor(m_self.lock());
    m_presentation->updateDecorations();
    layoutTarget()->recalc();

    // do animations
    m_realPosition->setConfig(Config::animationTree()->getAnimationPropertyConfig("windowsIn"));
    m_realSize->setConfig(Config::animationTree()->getAnimationPropertyConfig("windowsIn"));
    m_presentation->alpha(WINDOW_ALPHA_FADE)->setConfig(Config::animationTree()->getAnimationPropertyConfig("fadeIn"));
    m_presentation->applyAnimateIn();

    m_realPosition->setCallbackOnEnd(setVector2DAnimToMove);
    m_realSize->setCallbackOnEnd(setVector2DAnimToMove);

    // recalc the values for this window
    m_presentation->refreshValues();
    // avoid this window being visible if it's not the current covering FS window in workspace
    if (PWORKSPACE && Fullscreen::controller()->hasFullscreen(PWORKSPACE) && Fullscreen::controller()->getFullscreenWindow(PWORKSPACE, true) != m_self.lock() &&
        !m_target->floating()) {
        m_fullscreenPolicy->setAllowedOverFullscreen(false);
        updateFullscreenInputState();
        m_presentation->alpha(WINDOW_ALPHA_FULLSCREEN)->setValueAndWarp(0.f);
    }

    if (g_pSeatManager->m_mouse.expired() || !g_pInputManager->isConstrained())
        g_pInputManager->sendMotionEventsToFocused();

    if (m_workspace)
        m_workspace->updateWindows();

    Event::bus()->m_events.window.openLate.emit(m_self.lock());
}

void CWindow::unmapWindow() {
    Log::logger->log(Log::DEBUG, "{:c} unmapped", m_self.lock());

    static auto PEXITRETAINSFS = CConfigValue<Config::INTEGER>("misc:exit_window_retains_fullscreen");

    const bool  IS_CURRENT_WINDOW_FS      = Fullscreen::controller()->isFullscreen(m_self.lock());
    const auto  CURRENT_WINDOW_FS_MODES   = Fullscreen::controller()->getFullscreenModes(m_self.lock());
    const bool  CURRENT_FS_LAYOUT_HANDLED = IS_CURRENT_WINDOW_FS ? Fullscreen::controller()->layoutManagedFS(m_self.lock()) : false;
    const bool  WAS_FOCUSED               = m_self.lock() == Desktop::focusState()->window();
    if (!wlSurface()->exists() || !m_isMapped) {
        Log::logger->log(Log::WARN, "{} unmapped without being mapped??", m_self.lock());
        return;
    }

    const auto PMONITOR = m_monitor.lock();

    m_events.unmap.emit();
    IPC::Socket2::sock()->postEvent({"closewindow", std::format("{:x}", m_self.lock())});
    Event::bus()->m_events.window.close.emit(m_self.lock());

    if (m_target->floating() && !m_backend->isX11() && m_ruleApplicator->persistentSize().valueOrDefault()) {
        Log::logger->log(Log::DEBUG, "storing floating size {}x{} for window {}::{} on close", m_realSize->value().x, m_realSize->value().y, m_metadata->appID(),
                         m_metadata->title());
        Desktop::floatState()->remember(m_self.lock(), m_realSize->value());
    }

    const auto SWALLOW_UNMAP_RESULT    = m_swallowing->onUnmap(IS_CURRENT_WINDOW_FS ? std::optional(CURRENT_WINDOW_FS_MODES.internal) : std::nullopt, CURRENT_FS_LAYOUT_HANDLED);
    const auto RESTORED_SWALLOW_WINDOW = SWALLOW_UNMAP_RESULT.restoredWindow;

    if (IS_CURRENT_WINDOW_FS) {
        if (SWALLOW_UNMAP_RESULT.transferredInternalFullscreen)
            Fullscreen::controller()->setFullscreenMode(m_self.lock(), std::nullopt, Fullscreen::FSMODE_NONE, std::nullopt, Fullscreen::FULLSCREEN_MUTATION_TRANSFER);
        else
            Fullscreen::controller()->setFullscreenMode(m_self.lock(), Fullscreen::FSMODE_NONE, Fullscreen::FSMODE_NONE);
    }

    // Allow the renderer to catch the last frame.
    const auto SNAPSHOT =
        g_pHyprRenderer->shouldRenderWindow(m_self.lock()) && !m_ruleApplicator->noAnim().valueOrDefault() ? g_pHyprRenderer->makeSnapshotFB(m_self.lock()) : nullptr;

    bool      wasLastWindow = WAS_FOCUSED;
    PHLWINDOW nextInGroup   = RESTORED_SWALLOW_WINDOW ? RESTORED_SWALLOW_WINDOW : [this] -> PHLWINDOW {
        if (!m_grouping->group())
            return nullptr;

        // walk the history to find a suitable window
        const auto HISTORY = Desktop::History::windowTracker()->fullHistory();
        for (const auto& w : HISTORY | std::views::reverse) {
            if (!w || !w->m_isMapped || w == m_self)
                continue;

            if (!m_grouping->group()->has(w.lock()))
                continue;

            return w.lock();
        }

        return nullptr;
    }();

    if (wasLastWindow) {
        if (m_self.lock() == Desktop::focusState()->window())
            Desktop::focusState()->resetWindowFocus();

        g_pInputManager->releaseAllMouseButtons();
    }

    if (layoutTarget() == g_layoutManager->dragController()->target())
        g_layoutManager->endDragTarget();

    if (m_grouping->group())
        m_grouping->group()->remove(m_self.lock(), Math::DIRECTION_DEFAULT, CGroup::REMOVE_FROM_GROUP_REASON_UNMAP_WINDOW);

    g_layoutManager->removeTarget(m_target);

    g_pHyprRenderer->damageWindow(m_self.lock());

    // do this after onWindowRemoved because otherwise it'll think the window is invalid
    m_isMapped = false;

    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_MAPPED);

    // refocus on a new window if needed
    if (wasLastWindow) {
        static auto FOCUSONCLOSE = CConfigValue<Config::INTEGER>("input:focus_on_close");
        PHLWINDOW   candidate    = nextInGroup;

        if (!candidate) {
            if (*FOCUSONCLOSE == 1)
                candidate = (Desktop::viewState()->hitTest().windowAt(g_pInputManager->getMouseCoordsInternal(),
                                                                      Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING));
            else {
                const auto CAND = g_layoutManager->getNextCandidate(m_workspace->m_space, layoutTarget());
                if (CAND)
                    candidate = CAND->window();
            }
        }

        if (candidate && PMONITOR && PMONITOR->m_activeSpecialWorkspace && candidate->m_workspace != PMONITOR->m_activeSpecialWorkspace)
            candidate = nullptr;

        Log::logger->log(Log::DEBUG, "On closed window, new focused candidate is {}", candidate);

        if (candidate != Desktop::focusState()->window() && candidate) {
            if (candidate == nextInGroup)
                Desktop::focusState()->rawWindowFocus(candidate, FOCUS_REASON_UNMAP_GROUPED_WINDOW);
            else
                Desktop::focusState()->fullWindowFocus(candidate, m_target->floating() ? FOCUS_REASON_UNMAP_WINDOW_FLOATING : FOCUS_REASON_UNMAP_WINDOW_TILING);

            if ((*PEXITRETAINSFS == 1 || (candidate != nextInGroup && *PEXITRETAINSFS == 3) || (candidate == nextInGroup && *PEXITRETAINSFS == 2)) && IS_CURRENT_WINDOW_FS)
                // set the candidate to the current window's FS state - use the current window's layoutAware FS behaviour
                Fullscreen::controller()->setFullscreenMode(candidate, CURRENT_WINDOW_FS_MODES.internal, std::nullopt, CURRENT_FS_LAYOUT_HANDLED);
        }

        if (!candidate && m_workspace && (m_workspace->getWindowCount() == 0 || PMONITOR->m_activeSpecialWorkspace))
            g_pInputManager->refocus();

        g_pInputManager->sendMotionEventsToFocused();

        // CWindow::onUnmap will remove this window's active status, but we can't really do it above.
        if (m_self.lock() == Desktop::focusState()->window() || !Desktop::focusState()->window()) {
            IPC::Socket2::sock()->postEvent({"activewindow", ","});
            IPC::Socket2::sock()->postEvent({"activewindowv2", ""});

            Event::bus()->m_events.window.active.emit(m_self.lock(), FOCUS_REASON_OTHER);
        }
    } else {
        Log::logger->log(Log::DEBUG, "Unmapped was not focused, ignoring a refocus.");
    }

    if (!m_backend->traits().suggestsNoBorder)                              // don't animate out if they weren't animated in.
        *m_realPosition = m_realPosition->value() + Vector2D(0.01f, 0.01f); // it has to be animated, otherwise CesktopAnimationManager will ignore it

    const float FADEOUTALPHA =
        m_presentation->alphaValue(WINDOW_ALPHA_FADE) * m_presentation->alphaValue(WINDOW_ALPHA_FULLSCREEN) * m_presentation->alphaValue(WINDOW_ALPHA_LAYOUT);

    // FIXME: this shouldn't be needed but it is because style is decided by the fuckin anim from this
    m_realPosition->setConfig(Config::animationTree()->getAnimationPropertyConfig("windowsOut"));
    m_realSize->setConfig(Config::animationTree()->getAnimationPropertyConfig("windowsOut"));
    m_presentation->alpha(WINDOW_ALPHA_FADE)->setConfig(Config::animationTree()->getAnimationPropertyConfig("fadeOut"));

    Desktop::fadingOutState()->add(CWindowFadeout::create(m_self.lock(), SNAPSHOT, FADEOUTALPHA));

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    // force report all sizes (QT sometimes has an issue with this)
    if (m_workspace)
        m_workspace->forceReportSizesToWindows();

    // update lastwindow after focus
    onUnmap();
}

void CWindow::commitWindow(bool initialCommit) {
    if (!m_backend->valid())
        return;

    if (initialCommit) {
        // try to calculate static rules already for any floats
        m_ruleApplicator->readStaticRules(true);

        const Vector2D predSize = !m_ruleApplicator->static_.floating.value_or(false) // no float rule
                && !m_target->floating()                                              // not floating
                && !m_backend->parent()                                               // no parents
                && !suggestsFloat(true)                                               // should not be floated
            ?
            g_layoutManager->predictSizeForNewTiledTarget().value_or(Vector2D{}) :
            Vector2D{};

        Log::logger->log(Log::DEBUG, "Layout predicts size {} for {}", predSize, m_self.lock());

        m_backend->configure(CBox{{}, predSize}, m_monitor.lock());
        return;
    }

    if (!m_isMapped || isHidden())
        return;

    if (!m_backend->isX11() && !Fullscreen::controller()->isFullscreen(m_self.lock()) && m_target->floating()) {
        const auto HINTS = m_backend->geometryHints(eBackendState::BACKEND_STATE_CURRENT);
        if (clampWindowSize(HINTS.minSize, HINTS.maxSize))
            g_pHyprRenderer->damageWindow(m_self.lock());
    }

    if (!m_workspace->m_visible)
        return;

    const auto PMONITOR = m_monitor.lock();

    // damageSurface consumes damage, so snapshot it for the tearing check below
    const bool HADDAMAGE = !wlSurface()->resource()->m_current.damage.empty() || !wlSurface()->resource()->m_current.bufferDamage.empty();

    g_pHyprRenderer->damageSurface(wlSurface()->resource(), m_realPosition->goal().x, m_realPosition->goal().y, 1.0 / m_backend->surfaceScale());

    if (!m_backend->isX11()) {
        subsurfaceHead()->recheckDamageForSubsurfaces();
        popupHead()->recheckTree();
    }

    // tearing: if solitary, redraw it. This still might be a single surface window
    if (PMONITOR && PMONITOR->m_solitaryClient.lock() == m_self.lock() && canBeTorn() && PMONITOR->m_tearingState.canTear && wlSurface()->resource()->m_current.texture &&
        !PMONITOR->isTearingBlocked() && HADDAMAGE) {

        if (PMONITOR->m_tearingState.busy)
            PMONITOR->m_tearingState.frameScheduledWhileBusy = true;
        else {
            PMONITOR->m_tearingState.nextRenderTorn = true;
            g_pHyprRenderer->renderMonitor(PMONITOR);
        }
    }
}

void CWindow::destroyWindow() {
    Log::logger->log(Log::DEBUG, "{:c} destroyed, queueing.", m_self.lock());

    m_swallowing->onDestroy();

    if (m_self.lock() == Desktop::focusState()->window()) {
        Desktop::focusState()->window().reset();
        Desktop::focusState()->surface().reset();
    }

    wlSurface()->unassign();

    m_backendListeners = {};

    g_layoutManager->removeTarget(m_target);

    Desktop::windowState()->removeSafe(m_self.lock());
}

void CWindow::onActivationRequest() {
    Log::logger->log(Log::DEBUG, "X11 Activate request for window {}", m_self.lock());

    const auto TRAITS = m_backend->traits();
    if (TRAITS.overrideRedirect) {

        Log::logger->log(Log::DEBUG, "Unmanaged X11 {} requests activate", m_self.lock());

        if (Desktop::focusState()->window() && Desktop::focusState()->window()->backend().pid() != m_backend->pid())
            return;

        if (!TRAITS.wantsFocus)
            return;

        Desktop::focusState()->fullWindowFocus(m_self.lock(), FOCUS_REASON_DESKTOP_STATE_CHANGE);
        return;
    }

    if (m_self.lock() == Desktop::focusState()->window() || m_requestSuppression.activate)
        return;

    activate();
}

void CWindow::onMoveRequest() {
    if (!m_isMapped || isHidden() || g_layoutManager->dragController()->target())
        return;

    if (m_ruleApplicator->noXdgDrags().valueOrDefault())
        return;

    g_layoutManager->beginDragTarget(layoutTarget(), MBIND_MOVE, std::nullopt, true);
}

void CWindow::onResizeRequest(eBackendResizeEdge edge) {
    if (!m_isMapped || isHidden() || g_layoutManager->dragController()->target())
        return;

    if (m_ruleApplicator->noXdgDrags().valueOrDefault())
        return;

    g_layoutManager->beginDragTarget(layoutTarget(), MBIND_RESIZE, backendResizeEdgeToCorner(edge), true);
}

void CWindow::onGeometryChanged(const CBox& box) {
    if (m_backend->isX11() && m_backend->traits().overrideRedirect)
        unmanagedSetGeometry(box);
}

void CWindow::unmanagedSetGeometry(const CBox& box) {
    if (!m_isMapped || !m_backend->traits().overrideRedirect)
        return;

    const auto POS = m_realPosition->goal();
    const auto SIZ = m_realSize->goal();

    if (box.size() > Vector2D{1, 1})
        setHidden(false);
    else
        setHidden(true);

    if (Fullscreen::controller()->isFullscreen(m_self.lock()) || !m_target->floating()) {
        sendWindowSize(true);
        g_pHyprRenderer->damageWindow(m_self.lock());
        return;
    }

    if (abs(std::floor(POS.x) - box.x) > 2 || abs(std::floor(POS.y) - box.y) > 2 || abs(std::floor(SIZ.x) - box.w) > 2 || abs(std::floor(SIZ.y) - box.h) > 2) {
        Log::logger->log(Log::DEBUG, "Unmanaged window {} requests geometry update to {:j} {:j}", m_self.lock(), box.pos(), box.size());

        g_pHyprRenderer->damageWindow(m_self.lock());

        acknowledgeClientGeometry(box);
        layoutTarget()->setPositionGlobal(box, Layout::TARGET_UPDATE_NO_CLIENT_CONFIGURE);
        // This is an X11-confirmed geometry update. Keeping the animation would leave
        // animated effects, like shadows, briefly at the old geometry as phantoms.
        layoutTarget()->warpPositionSize();

        m_workspace = State::monitorState()->query().vec(m_realPosition->value() + m_realSize->value() / 2.f).run()->m_activeWorkspace;

        Desktop::windowState()->raise(m_self.lock());
        m_presentation->updateDecorations();
        g_pHyprRenderer->damageWindow(m_self.lock());
    }
}

std::optional<Vector2D> CWindow::minSize() {
    return m_target->minSize();
}

std::optional<Vector2D> CWindow::maxSize() {
    return m_target->maxSize();
}

SP<Layout::CWindowTarget> CWindow::windowTarget() {
    return m_target;
}

SP<Layout::CWindowTarget> CWindow::windowTarget() const {
    return m_target;
}

SP<Layout::ITarget> CWindow::layoutTarget() {
    return std::as_const(*this).layoutTarget();
}

SP<Layout::ITarget> CWindow::layoutTarget() const {
    if (m_grouping->group())
        return m_grouping->group()->target();

    return m_target;
}

bool CWindow::isFloating() const {
    return m_target->floating();
}

bool CWindow::cantLockCursor() const {
    return m_target->cantLockCursor();
}

CWindowGroupMembership& CWindow::grouping() {
    return *m_grouping;
}

const CWindowGroupMembership& CWindow::grouping() const {
    return *m_grouping;
}

void CWindow::sendClose() {
    if (m_isMapped)
        m_backend->close();
}

Types::CMultiAVarContainer<float, uint8_t>& CWindow::alpha() {
    return m_presentation->alpha();
}

const Types::CMultiAVarContainer<float, uint8_t>& CWindow::alpha() const {
    return m_presentation->alpha();
}

std::optional<uint8_t> CWindow::alphaGenericToKey(eAlphaModifiableProp p) {
    switch (p) {
        case IAlphaModifiable::ALPHA_MODIFIABLE_FADE: return WINDOW_ALPHA_FADE;

        // this is here to suppress the warning
        case IAlphaModifiable::ALPHA_MODIFIABLE_LAST: return std::nullopt;
    }

    static_assert(ALPHA_MODIFIABLE_LAST == 1);
    UNREACHABLE();
}
