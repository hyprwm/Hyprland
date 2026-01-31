#include <algorithm>
#include <ranges>
#include <hyprutils/animation/AnimatedVariable.hpp>
#include <re2/re2.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <any>
#include <bit>
#include <fstream>
#include <string_view>
#include "Window.hpp"
#include "LayerSurface.hpp"
#include "../state/FocusState.hpp"
#include "../history/WindowHistoryTracker.hpp"
#include "../../Compositor.hpp"
#include "../../render/decorations/CHyprDropShadowDecoration.hpp"
#include "../../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../../render/decorations/CHyprBorderDecoration.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../managers/TokenManager.hpp"
#include "../../managers/animation/AnimationManager.hpp"
#include "../../managers/ANRManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../protocols/XDGShell.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/core/Subcompositor.hpp"
#include "../../protocols/ContentType.hpp"
#include "../../protocols/FractionalScale.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../xwayland/XWayland.hpp"
#include "../../helpers/Color.hpp"
#include "../../helpers/math/Expression.hpp"
#include "../../managers/XWaylandManager.hpp"
#include "../../render/Renderer.hpp"
#include "../../managers/LayoutManager.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../managers/EventManager.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../managers/PointerManager.hpp"
#include "../../managers/animation/DesktopAnimationManager.hpp"

#include <hyprutils/string/String.hpp>

using namespace Hyprutils::String;
using namespace Hyprutils::Animation;
using enum NContentType::eContentType;

using namespace Desktop;
using namespace Desktop::View;

PHLWINDOW CWindow::create(SP<CXWaylandSurface> surface) {
    PHLWINDOW pWindow = SP<CWindow>(new CWindow(surface));

    pWindow->m_self           = pWindow;
    pWindow->m_isX11          = true;
    pWindow->m_ruleApplicator = makeUnique<Desktop::Rule::CWindowRuleApplicator>(pWindow);

    g_pAnimationManager->createAnimation(Vector2D(0, 0), pWindow->m_realPosition, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(Vector2D(0, 0), pWindow->m_realSize, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_borderFadeAnimationProgress, g_pConfigManager->getAnimationPropertyConfig("border"), pWindow, AVARDAMAGE_BORDER);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_borderAngleAnimationProgress, g_pConfigManager->getAnimationPropertyConfig("borderangle"), pWindow, AVARDAMAGE_BORDER);
    g_pAnimationManager->createAnimation(1.f, pWindow->m_alpha, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(1.f, pWindow->m_activeInactiveAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeSwitch"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(CHyprColor(), pWindow->m_realShadowColor, g_pConfigManager->getAnimationPropertyConfig("fadeShadow"), pWindow, AVARDAMAGE_SHADOW);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_dimPercent, g_pConfigManager->getAnimationPropertyConfig("fadeDim"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_movingToWorkspaceAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeOut"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_movingFromWorkspaceAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_notRespondingTint, g_pConfigManager->getAnimationPropertyConfig("fade"), pWindow, AVARDAMAGE_ENTIRE);

    pWindow->addWindowDeco(makeUnique<CHyprDropShadowDecoration>(pWindow));
    pWindow->addWindowDeco(makeUnique<CHyprBorderDecoration>(pWindow));

    return pWindow;
}

PHLWINDOW CWindow::create(SP<CXDGSurfaceResource> resource) {
    PHLWINDOW pWindow = SP<CWindow>(new CWindow(resource));

    pWindow->m_self                = pWindow;
    resource->m_toplevel->m_window = pWindow;
    pWindow->m_ruleApplicator      = makeUnique<Desktop::Rule::CWindowRuleApplicator>(pWindow);

    g_pAnimationManager->createAnimation(Vector2D(0, 0), pWindow->m_realPosition, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(Vector2D(0, 0), pWindow->m_realSize, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_borderFadeAnimationProgress, g_pConfigManager->getAnimationPropertyConfig("border"), pWindow, AVARDAMAGE_BORDER);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_borderAngleAnimationProgress, g_pConfigManager->getAnimationPropertyConfig("borderangle"), pWindow, AVARDAMAGE_BORDER);
    g_pAnimationManager->createAnimation(1.f, pWindow->m_alpha, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(1.f, pWindow->m_activeInactiveAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeSwitch"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(CHyprColor(), pWindow->m_realShadowColor, g_pConfigManager->getAnimationPropertyConfig("fadeShadow"), pWindow, AVARDAMAGE_SHADOW);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_dimPercent, g_pConfigManager->getAnimationPropertyConfig("fadeDim"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_movingToWorkspaceAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeOut"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_movingFromWorkspaceAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_notRespondingTint, g_pConfigManager->getAnimationPropertyConfig("fade"), pWindow, AVARDAMAGE_ENTIRE);

    pWindow->addWindowDeco(makeUnique<CHyprDropShadowDecoration>(pWindow));
    pWindow->addWindowDeco(makeUnique<CHyprBorderDecoration>(pWindow));

    pWindow->wlSurface()->assign(pWindow->m_xdgSurface->m_surface.lock(), pWindow);

    return pWindow;
}

CWindow::CWindow(SP<CXDGSurfaceResource> resource) : IView(CWLSurface::create()), m_xdgSurface(resource) {
    m_listeners.map            = m_xdgSurface->m_events.map.listen([this] { mapWindow(); });
    m_listeners.ack            = m_xdgSurface->m_events.ack.listen([this](uint32_t d) { onAck(d); });
    m_listeners.unmap          = m_xdgSurface->m_events.unmap.listen([this] { unmapWindow(); });
    m_listeners.destroy        = m_xdgSurface->m_events.destroy.listen([this] { destroyWindow(); });
    m_listeners.commit         = m_xdgSurface->m_events.commit.listen([this] { commitWindow(); });
    m_listeners.updateState    = m_xdgSurface->m_toplevel->m_events.stateChanged.listen([this] { onUpdateState(); });
    m_listeners.updateMetadata = m_xdgSurface->m_toplevel->m_events.metadataChanged.listen([this] { onUpdateMeta(); });
}

CWindow::CWindow(SP<CXWaylandSurface> surface) : IView(CWLSurface::create()), m_xwaylandSurface(surface) {
    m_listeners.map              = m_xwaylandSurface->m_events.map.listen([this] { mapWindow(); });
    m_listeners.unmap            = m_xwaylandSurface->m_events.unmap.listen([this] { unmapWindow(); });
    m_listeners.destroy          = m_xwaylandSurface->m_events.destroy.listen([this] { destroyWindow(); });
    m_listeners.commit           = m_xwaylandSurface->m_events.commit.listen([this] { commitWindow(); });
    m_listeners.configureRequest = m_xwaylandSurface->m_events.configureRequest.listen([this](const CBox& box) { onX11ConfigureRequest(box); });
    m_listeners.updateState      = m_xwaylandSurface->m_events.stateChanged.listen([this] { onUpdateState(); });
    m_listeners.updateMetadata   = m_xwaylandSurface->m_events.metadataChanged.listen([this] { onUpdateMeta(); });
    m_listeners.resourceChange   = m_xwaylandSurface->m_events.resourceChange.listen([this] { onResourceChangeX11(); });
    m_listeners.activate         = m_xwaylandSurface->m_events.activate.listen([this] { activateX11(); });

    if (m_xwaylandSurface->m_overrideRedirect)
        m_listeners.setGeometry = m_xwaylandSurface->m_events.setGeometry.listen([this] { unmanagedSetGeometry(); });
}

SP<CWindow> CWindow::fromView(SP<IView> v) {
    if (!v || v->type() != VIEW_TYPE_WINDOW)
        return nullptr;
    return dynamicPointerCast<CWindow>(v);
}

CWindow::~CWindow() {
    if (Desktop::focusState()->window() == m_self) {
        Desktop::focusState()->surface().reset();
        Desktop::focusState()->window().reset();
    }

    m_events.destroy.emit();

    if (!g_pHyprOpenGL)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_windowFramebuffers, [&](const auto& other) { return other.first.expired() || other.first.get() == this; });
}

eViewType CWindow::type() const {
    return VIEW_TYPE_WINDOW;
}

bool CWindow::visible() const {
    return !m_hidden && ((m_isMapped && m_wlSurface && m_wlSurface->resource()) || (m_fadingOut && m_alpha->value() != 0.F));
}

std::optional<CBox> CWindow::logicalBox() const {
    return getFullWindowBoundingBox();
}

bool CWindow::desktopComponent() const {
    return true;
}

std::optional<CBox> CWindow::surfaceLogicalBox() const {
    return getWindowMainSurfaceBox();
}

SBoxExtents CWindow::getFullWindowExtents() const {
    if (m_fadingOut)
        return m_originalClosedExtents;

    const int BORDERSIZE = getRealBorderSize();

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

    if (m_wlSurface->exists() && !m_isX11 && m_popupHead) {
        CBox surfaceExtents = {0, 0, 0, 0};
        // TODO: this could be better, perhaps make a getFullWindowRegion?
        m_popupHead->breadthfirst(
            [](WP<Desktop::View::CPopup> popup, void* data) {
                if (!popup->wlSurface() || !popup->wlSurface()->resource())
                    return;

                CBox* pSurfaceExtents = sc<CBox*>(data);
                CBox  surf            = CBox{popup->coordsRelativeToParent(), popup->size()};
                pSurfaceExtents->x    = std::min(surf.x, pSurfaceExtents->x);
                pSurfaceExtents->y    = std::min(surf.y, pSurfaceExtents->y);
                if (surf.x + surf.w > pSurfaceExtents->width)
                    pSurfaceExtents->width = surf.x + surf.w - pSurfaceExtents->x;
                if (surf.y + surf.h > pSurfaceExtents->height)
                    pSurfaceExtents->height = surf.y + surf.h - pSurfaceExtents->y;
            },
            &surfaceExtents);

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

    CBox finalBox = {m_realPosition->value().x - maxExtents.topLeft.x, m_realPosition->value().y - maxExtents.topLeft.y,
                     m_realSize->value().x + maxExtents.topLeft.x + maxExtents.bottomRight.x, m_realSize->value().y + maxExtents.topLeft.y + maxExtents.bottomRight.y};

    return finalBox;
}

CBox CWindow::getWindowIdealBoundingBoxIgnoreReserved() {
    const auto PMONITOR = m_monitor.lock();

    if (!PMONITOR || !m_workspace)
        return {m_position, m_size};

    auto POS  = m_position;
    auto SIZE = m_size;

    if (isFullscreen()) {
        POS  = PMONITOR->m_position;
        SIZE = PMONITOR->m_size;

        return CBox{sc<int>(POS.x), sc<int>(POS.y), sc<int>(SIZE.x), sc<int>(SIZE.y)};
    }

    // get work area
    const auto WORKAREA = g_pLayoutManager->getCurrentLayout()->workAreaOnWorkspace(m_workspace);
    const auto RESERVED = CReservedArea{PMONITOR->logicalBox(), WORKAREA};

    if (DELTALESSTHAN(POS.y - PMONITOR->m_position.y, RESERVED.top(), 1)) {
        POS.y = PMONITOR->m_position.y;
        SIZE.y += RESERVED.top();
    }
    if (DELTALESSTHAN(POS.x - PMONITOR->m_position.x, RESERVED.left(), 1)) {
        POS.x = PMONITOR->m_position.x;
        SIZE.x += RESERVED.left();
    }

    if (DELTALESSTHAN(POS.x + SIZE.x - PMONITOR->m_position.x, PMONITOR->m_size.x - RESERVED.right(), 1))
        SIZE.x += RESERVED.right();

    if (DELTALESSTHAN(POS.y + SIZE.y - PMONITOR->m_position.y, PMONITOR->m_size.y - RESERVED.bottom(), 1))
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

    const auto POS  = m_realPosition->value();
    const auto SIZE = m_realSize->value();

    CBox       box{POS, SIZE};
    box.addExtents(getWindowExtentsUnified(properties));

    return box;
}

SBoxExtents CWindow::getFullWindowReservedArea() {
    return g_pDecorationPositioner->getWindowDecorationReserved(m_self);
}

void CWindow::updateWindowDecos() {

    if (!m_isMapped || isHidden())
        return;

    for (auto const& wd : m_decosToRemove) {
        for (auto it = m_windowDecorations.begin(); it != m_windowDecorations.end(); it++) {
            if (it->get() == wd) {
                g_pDecorationPositioner->uncacheDecoration(it->get());
                it = m_windowDecorations.erase(it);
                if (it == m_windowDecorations.end())
                    break;
            }
        }
    }

    g_pDecorationPositioner->onWindowUpdate(m_self.lock());

    m_decosToRemove.clear();

    // make a copy because updateWindow can remove decos.
    std::vector<IHyprWindowDecoration*> decos;
    // reserve to avoid reallocations
    decos.reserve(m_windowDecorations.size());

    for (auto const& wd : m_windowDecorations) {
        decos.push_back(wd.get());
    }

    for (auto const& wd : decos) {
        if (std::ranges::find_if(m_windowDecorations, [wd](const auto& other) { return other.get() == wd; }) == m_windowDecorations.end())
            continue;
        wd->updateWindow(m_self.lock());
    }
}

void CWindow::addWindowDeco(UP<IHyprWindowDecoration> deco) {
    m_windowDecorations.emplace_back(std::move(deco));
    g_pDecorationPositioner->forceRecalcFor(m_self.lock());
    updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(m_self.lock());
}

void CWindow::removeWindowDeco(IHyprWindowDecoration* deco) {
    m_decosToRemove.push_back(deco);
    g_pDecorationPositioner->forceRecalcFor(m_self.lock());
    updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(m_self.lock());
}

void CWindow::uncacheWindowDecos() {
    for (auto const& wd : m_windowDecorations) {
        g_pDecorationPositioner->uncacheDecoration(wd.get());
    }
}

bool CWindow::checkInputOnDecos(const eInputType type, const Vector2D& mouseCoords, std::any data) {
    if (type != INPUT_TYPE_DRAG_END && hasPopupAt(mouseCoords))
        return false;

    for (auto const& wd : m_windowDecorations) {
        if (!(wd->getDecorationFlags() & DECORATION_ALLOWS_MOUSE_INPUT))
            continue;

        if (!g_pDecorationPositioner->getWindowDecorationBox(wd.get()).containsPoint(mouseCoords))
            continue;

        if (wd->onInputOnDeco(type, mouseCoords, data))
            return true;
    }

    return false;
}

pid_t CWindow::getPID() {
    pid_t PID = -1;
    if (!m_isX11) {
        if (!m_xdgSurface || !m_xdgSurface->m_owner /* happens at unmap */)
            return -1;

        wl_client_get_credentials(m_xdgSurface->m_owner->client(), &PID, nullptr, nullptr);
    } else {
        if (!m_xwaylandSurface)
            return -1;

        PID = m_xwaylandSurface->m_pid;
    }

    return PID;
}

IHyprWindowDecoration* CWindow::getDecorationByType(eDecorationType type) {
    for (auto const& wd : m_windowDecorations) {
        if (wd->getDecorationType() == type)
            return wd.get();
    }

    return nullptr;
}

void CWindow::updateToplevel() {
    updateSurfaceScaleTransformDetails();
}

void CWindow::updateSurfaceScaleTransformDetails(bool force) {
    if (!m_isMapped || m_hidden || g_pCompositor->m_unsafeState)
        return;

    const auto PLASTMONITOR = g_pCompositor->getMonitorFromID(m_lastSurfaceMonitorID);

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
            if (PSURFACE && PSURFACE->m_lastScaleFloat == PMONITOR->m_scale)
                return;

            PROTO::fractional->sendScale(s, PMONITOR->m_scale);
            g_pCompositor->setPreferredScaleForSurface(s, PMONITOR->m_scale);
            g_pCompositor->setPreferredTransformForSurface(s, PMONITOR->m_transform);
        },
        nullptr);
}

void CWindow::moveToWorkspace(PHLWORKSPACE pWorkspace) {
    if (m_workspace == pWorkspace)
        return;

    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

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

    static auto PCLOSEONLASTSPECIAL = CConfigValue<Hyprlang::INT>("misc:close_special_on_empty");

    const auto  OLDWORKSPACE = m_workspace;

    if (OLDWORKSPACE->isVisible()) {
        m_movingToWorkspaceAlpha->setValueAndWarp(1.F);
        *m_movingToWorkspaceAlpha = 0.F;
        m_movingToWorkspaceAlpha->setCallbackOnEnd([this](auto) { m_monitorMovedFrom = -1; });
        m_monitorMovedFrom = OLDWORKSPACE ? OLDWORKSPACE->monitorID() : -1;
    }

    m_workspace = pWorkspace;

    setAnimationsToMove();

    OLDWORKSPACE->updateWindows();
    OLDWORKSPACE->updateWindowData();
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(OLDWORKSPACE->monitorID());

    pWorkspace->updateWindows();
    pWorkspace->updateWindowData();
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (valid(pWorkspace)) {
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "movewindow", .data = std::format("{:x},{}", rc<uintptr_t>(this), pWorkspace->m_name)});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "movewindowv2", .data = std::format("{:x},{},{}", rc<uintptr_t>(this), pWorkspace->m_id, pWorkspace->m_name)});
        EMIT_HOOK_EVENT("moveWindow", (std::vector<std::any>{m_self.lock(), pWorkspace}));
    }

    if (const auto SWALLOWED = m_swallowed.lock()) {
        if (SWALLOWED->m_currentlySwallowed) {
            SWALLOWED->moveToWorkspace(pWorkspace);
            SWALLOWED->m_monitor = m_monitor;
        }
    }

    if (OLDWORKSPACE && g_pCompositor->isWorkspaceSpecial(OLDWORKSPACE->m_id) && OLDWORKSPACE->getWindows() == 0 && *PCLOSEONLASTSPECIAL) {
        if (const auto PMONITOR = OLDWORKSPACE->m_monitor.lock(); PMONITOR)
            PMONITOR->setSpecialWorkspace(nullptr);
    }
}

PHLWINDOW CWindow::x11TransientFor() {
    if (!m_xwaylandSurface || !m_xwaylandSurface->m_parent)
        return nullptr;

    auto                              s = m_xwaylandSurface->m_parent;
    std::vector<SP<CXWaylandSurface>> visited;
    while (s) {
        // break loops. Some X apps make them, and it seems like it's valid behavior?!?!?!
        // TODO: we should reject loops being created in the first place.
        if (std::ranges::find(visited.begin(), visited.end(), s) != visited.end())
            break;

        visited.emplace_back(s.lock());
        s = s->m_parent;
    }

    if (s == m_xwaylandSurface)
        return nullptr; // dead-ass circle

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_xwaylandSurface != s)
            continue;
        return w;
    }

    return nullptr;
}

void CWindow::onUnmap() {
    static auto PCLOSEONLASTSPECIAL = CConfigValue<Hyprlang::INT>("misc:close_special_on_empty");
    static auto PINITIALWSTRACKING  = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

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
    if (m_workspace->m_isSpecialWorkspace && m_workspace->getWindows() == 0)
        m_lastWorkspace = m_monitor->activeWorkspaceID();

    if (*PCLOSEONLASTSPECIAL && m_workspace && m_workspace->getWindows() == 0 && onSpecialWorkspace()) {
        const auto PMONITOR = m_monitor.lock();
        if (PMONITOR && PMONITOR->m_activeSpecialWorkspace && PMONITOR->m_activeSpecialWorkspace == m_workspace)
            PMONITOR->setSpecialWorkspace(nullptr);
    }

    const auto PMONITOR = m_monitor.lock();

    if (PMONITOR && PMONITOR->m_solitaryClient == m_self)
        PMONITOR->m_solitaryClient.reset();

    if (m_workspace) {
        m_workspace->updateWindows();
        m_workspace->updateWindowData();
    }
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    m_workspace.reset();

    if (m_isX11)
        return;

    m_subsurfaceHead.reset();
    m_popupHead.reset();
}

void CWindow::onMap() {
    // JIC, reset the callbacks. If any are set, we'll make sure they are cleared so we don't accidentally unset them. (In case a window got remapped)
    m_realPosition->resetAllCallbacks();
    m_realSize->resetAllCallbacks();
    m_borderFadeAnimationProgress->resetAllCallbacks();
    m_borderAngleAnimationProgress->resetAllCallbacks();
    m_activeInactiveAlpha->resetAllCallbacks();
    m_alpha->resetAllCallbacks();
    m_realShadowColor->resetAllCallbacks();
    m_dimPercent->resetAllCallbacks();
    m_movingToWorkspaceAlpha->resetAllCallbacks();
    m_movingFromWorkspaceAlpha->resetAllCallbacks();

    m_movingFromWorkspaceAlpha->setValueAndWarp(1.F);

    if (m_borderAngleAnimationProgress->enabled()) {
        m_borderAngleAnimationProgress->setValueAndWarp(0.f);
        m_borderAngleAnimationProgress->setCallbackOnEnd([&](WP<CBaseAnimatedVariable> p) { onBorderAngleAnimEnd(p); }, false);
        *m_borderAngleAnimationProgress = 1.f;
    }

    m_realSize->setCallbackOnBegin(
        [this](auto) {
            if (!m_isMapped || isX11OverrideRedirect())
                return;

            g_pEventLoopManager->doLater([this, self = m_self] {
                if (!self)
                    return;

                sendWindowSize();
            });
        },
        false);

    m_movingFromWorkspaceAlpha->setValueAndWarp(1.F);

    m_reportedSize = m_pendingReportedSize;
    m_animatingIn  = true;

    updateSurfaceScaleTransformDetails(true);

    if (m_isX11)
        return;

    m_subsurfaceHead = CSubsurface::create(m_self.lock());
    m_popupHead      = CPopup::create(m_self.lock());
}

void CWindow::onBorderAngleAnimEnd(WP<CBaseAnimatedVariable> pav) {
    if (!pav)
        return;

    if (pav->getStyle() != "loop" || !pav->enabled())
        return;

    const auto PANIMVAR = dc<CAnimatedVariable<float>*>(pav.get());

    PANIMVAR->setCallbackOnEnd(nullptr); // we remove the callback here because otherwise setvalueandwarp will recurse this

    PANIMVAR->setValueAndWarp(0);
    *PANIMVAR = 1.f;

    PANIMVAR->setCallbackOnEnd([&](WP<CBaseAnimatedVariable> pav) { onBorderAngleAnimEnd(pav); }, false);
}

void CWindow::setHidden(bool hidden) {
    m_hidden = hidden;

    if (hidden && Desktop::focusState()->window() == m_self)
        Desktop::focusState()->window().reset();

    setSuspended(hidden);
}

bool CWindow::isHidden() {
    return m_hidden;
}

// check if the point is "hidden" under a rounded corner of the window
// it is assumed that the point is within the real window box (m_vRealPosition, m_vRealSize)
// otherwise behaviour is undefined
bool CWindow::isInCurvedCorner(double x, double y) {
    const int ROUNDING      = rounding();
    const int ROUNDINGPOWER = roundingPower();
    if (getRealBorderSize() >= ROUNDING)
        return false;

    // (x0, y0), (x0, y1), ... are the center point of rounding at each corner
    double x0 = m_realPosition->value().x + ROUNDING;
    double y0 = m_realPosition->value().y + ROUNDING;
    double x1 = m_realPosition->value().x + m_realSize->value().x - ROUNDING;
    double y1 = m_realPosition->value().y + m_realSize->value().y - ROUNDING;

    if (x < x0 && y < y0) {
        return std::pow(x0 - x, ROUNDINGPOWER) + std::pow(y0 - y, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);
    }
    if (x > x1 && y < y0) {
        return std::pow(x - x1, ROUNDINGPOWER) + std::pow(y0 - y, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);
    }
    if (x < x0 && y > y1) {
        return std::pow(x0 - x, ROUNDINGPOWER) + std::pow(y - y1, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);
    }
    if (x > x1 && y > y1) {
        return std::pow(x - x1, ROUNDINGPOWER) + std::pow(y - y1, ROUNDINGPOWER) > std::pow(sc<double>(ROUNDING), ROUNDINGPOWER);
    }

    return false;
}

// checks if the wayland window has a popup at pos
bool CWindow::hasPopupAt(const Vector2D& pos) {
    if (m_isX11)
        return false;

    auto popup = m_popupHead->at(pos);

    return popup && popup->wlSurface()->resource();
}

void CWindow::applyGroupRules() {
    if ((m_groupRules & GROUP_SET && m_firstMap) || m_groupRules & GROUP_SET_ALWAYS)
        createGroup();

    if (m_groupData.pNextWindow.lock() && ((m_groupRules & GROUP_LOCK && m_firstMap) || m_groupRules & GROUP_LOCK_ALWAYS))
        getGroupHead()->m_groupData.locked = true;
}

void CWindow::createGroup() {
    if (m_groupData.deny) {
        Log::logger->log(Log::DEBUG, "createGroup: window:{:x},title:{} is denied as a group, ignored", rc<uintptr_t>(this), this->m_title);
        return;
    }

    if (m_groupData.pNextWindow.expired()) {
        m_groupData.pNextWindow = m_self;
        m_groupData.head        = true;
        m_groupData.locked      = false;
        m_groupData.deny        = false;

        addWindowDeco(makeUnique<CHyprGroupBarDecoration>(m_self.lock()));

        if (m_workspace) {
            m_workspace->updateWindows();
            m_workspace->updateWindowData();
        }
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();

        g_pEventManager->postEvent(SHyprIPCEvent{.event = "togglegroup", .data = std::format("1,{:x}", rc<uintptr_t>(this))});
    }

    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
}

void CWindow::destroyGroup() {
    if (m_groupData.pNextWindow == m_self) {
        if (m_groupRules & GROUP_SET_ALWAYS) {
            Log::logger->log(Log::DEBUG, "destoryGroup: window:{:x},title:{} has rule [group set always], ignored", rc<uintptr_t>(this), this->m_title);
            return;
        }
        m_groupData.pNextWindow.reset();
        m_groupData.head = false;
        updateWindowDecos();
        if (m_workspace) {
            m_workspace->updateWindows();
            m_workspace->updateWindowData();
        }
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();

        g_pEventManager->postEvent(SHyprIPCEvent{.event = "togglegroup", .data = std::format("0,{:x}", rc<uintptr_t>(this))});
        m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        return;
    }

    std::string            addresses;
    PHLWINDOW              curr = m_self.lock();
    std::vector<PHLWINDOW> members;
    do {
        const auto PLASTWIN = curr;
        curr                = curr->m_groupData.pNextWindow.lock();
        PLASTWIN->m_groupData.pNextWindow.reset();
        curr->setHidden(false);
        members.push_back(curr);

        addresses += std::format("{:x},", rc<uintptr_t>(curr.get()));
    } while (curr.get() != this);

    for (auto const& w : members) {
        if (w->m_groupData.head)
            g_pLayoutManager->getCurrentLayout()->onWindowRemoved(curr);
        w->m_groupData.head = false;
    }

    const bool GROUPSLOCKEDPREV       = g_pKeybindManager->m_groupsLocked;
    g_pKeybindManager->m_groupsLocked = true;
    for (auto const& w : members) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(w);
        w->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        w->updateWindowDecos();
    }
    g_pKeybindManager->m_groupsLocked = GROUPSLOCKEDPREV;

    if (m_workspace) {
        m_workspace->updateWindows();
        m_workspace->updateWindowData();
    }
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (!addresses.empty())
        addresses.pop_back();

    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "togglegroup", .data = std::format("0,{}", addresses)});
}

PHLWINDOW CWindow::getGroupHead() {
    PHLWINDOW curr = m_self.lock();
    while (!curr->m_groupData.head)
        curr = curr->m_groupData.pNextWindow.lock();
    return curr;
}

PHLWINDOW CWindow::getGroupTail() {
    PHLWINDOW curr = m_self.lock();
    while (!curr->m_groupData.pNextWindow->m_groupData.head)
        curr = curr->m_groupData.pNextWindow.lock();
    return curr;
}

PHLWINDOW CWindow::getGroupCurrent() {
    PHLWINDOW curr = m_self.lock();
    while (curr->isHidden())
        curr = curr->m_groupData.pNextWindow.lock();
    return curr;
}

int CWindow::getGroupSize() {
    int       size = 1;
    PHLWINDOW curr = m_self.lock();
    while (curr->m_groupData.pNextWindow != m_self) {
        curr = curr->m_groupData.pNextWindow.lock();
        size++;
    }
    return size;
}

bool CWindow::canBeGroupedInto(PHLWINDOW pWindow) {
    static auto ALLOWGROUPMERGE       = CConfigValue<Hyprlang::INT>("group:merge_groups_on_drag");
    bool        isGroup               = m_groupData.pNextWindow;
    bool        disallowDragIntoGroup = g_pInputManager->m_wasDraggingWindow && isGroup && !sc<bool>(*ALLOWGROUPMERGE);
    return !g_pKeybindManager->m_groupsLocked                                                // global group lock disengaged
        && ((m_groupRules & GROUP_INVADE && m_firstMap)                                      // window ignore local group locks, or
            || (!pWindow->getGroupHead()->m_groupData.locked                                 //      target unlocked
                && !(m_groupData.pNextWindow.lock() && getGroupHead()->m_groupData.locked))) //      source unlocked or isn't group
        && !m_groupData.deny                                                                 // source is not denied entry
        && !(m_groupRules & GROUP_BARRED && m_firstMap)                                      // group rule doesn't prevent adding window
        && !disallowDragIntoGroup;                                                           // config allows groups to be merged
}

PHLWINDOW CWindow::getGroupWindowByIndex(int index) {
    const int SIZE = getGroupSize();
    index          = ((index % SIZE) + SIZE) % SIZE;
    PHLWINDOW curr = getGroupHead();
    while (index > 0) {
        curr = curr->m_groupData.pNextWindow.lock();
        index--;
    }
    return curr;
}

bool CWindow::hasInGroup(PHLWINDOW w) {
    PHLWINDOW curr = m_groupData.pNextWindow.lock();
    while (curr && curr != m_self) {
        if (curr == w)
            return true;
        curr = curr->m_groupData.pNextWindow.lock();
    }
    return false;
}

void CWindow::setGroupCurrent(PHLWINDOW pWindow) {
    PHLWINDOW curr     = m_groupData.pNextWindow.lock();
    bool      isMember = false;
    while (curr.get() != this) {
        if (curr == pWindow) {
            isMember = true;
            break;
        }
        curr = curr->m_groupData.pNextWindow.lock();
    }

    if (!isMember && pWindow.get() != this)
        return;

    const auto PCURRENT   = getGroupCurrent();
    const bool FULLSCREEN = PCURRENT->isFullscreen();
    const auto WORKSPACE  = PCURRENT->m_workspace;
    const auto MODE       = PCURRENT->m_fullscreenState.internal;

    const auto CURRENTISFOCUS = PCURRENT == Desktop::focusState()->window();

    const auto PWINDOWSIZE                 = PCURRENT->m_realSize->value();
    const auto PWINDOWPOS                  = PCURRENT->m_realPosition->value();
    const auto PWINDOWSIZEGOAL             = PCURRENT->m_realSize->goal();
    const auto PWINDOWPOSGOAL              = PCURRENT->m_realPosition->goal();
    const auto PWINDOWLASTFLOATINGSIZE     = PCURRENT->m_lastFloatingSize;
    const auto PWINDOWLASTFLOATINGPOSITION = PCURRENT->m_lastFloatingPosition;

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreenInternal(PCURRENT, FSMODE_NONE);

    PCURRENT->setHidden(true);
    pWindow->setHidden(false); // can remove m_pLastWindow

    g_pLayoutManager->getCurrentLayout()->replaceWindowDataWith(PCURRENT, pWindow);

    if (PCURRENT->m_isFloating) {
        pWindow->m_realPosition->setValueAndWarp(PWINDOWPOSGOAL);
        pWindow->m_realSize->setValueAndWarp(PWINDOWSIZEGOAL);
        pWindow->sendWindowSize();
    }

    pWindow->m_realPosition->setValue(PWINDOWPOS);
    pWindow->m_realSize->setValue(PWINDOWSIZE);

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreenInternal(pWindow, MODE);

    pWindow->m_lastFloatingSize     = PWINDOWLASTFLOATINGSIZE;
    pWindow->m_lastFloatingPosition = PWINDOWLASTFLOATINGPOSITION;

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (CURRENTISFOCUS)
        Desktop::focusState()->rawWindowFocus(pWindow);

    g_pHyprRenderer->damageWindow(pWindow);

    pWindow->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    pWindow->updateWindowDecos();
}

void CWindow::insertWindowToGroup(PHLWINDOW pWindow) {
    const auto BEGINAT = m_self.lock();
    const auto ENDAT   = m_groupData.pNextWindow.lock();

    if (!pWindow->m_groupData.pNextWindow.lock()) {
        BEGINAT->m_groupData.pNextWindow = pWindow;
        pWindow->m_groupData.pNextWindow = ENDAT;
        pWindow->m_groupData.head        = false;
        pWindow->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(pWindow));
        return;
    }

    const auto SHEAD = pWindow->getGroupHead();
    const auto STAIL = pWindow->getGroupTail();

    SHEAD->m_groupData.head          = false;
    BEGINAT->m_groupData.pNextWindow = SHEAD;
    STAIL->m_groupData.pNextWindow   = ENDAT;

    pWindow->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    pWindow->updateWindowDecos();
}

PHLWINDOW CWindow::getGroupPrevious() {
    PHLWINDOW curr = m_groupData.pNextWindow.lock();

    while (curr != m_self && curr->m_groupData.pNextWindow != m_self)
        curr = curr->m_groupData.pNextWindow.lock();

    return curr;
}

void CWindow::switchWithWindowInGroup(PHLWINDOW pWindow) {
    if (!m_groupData.pNextWindow.lock() || !pWindow->m_groupData.pNextWindow.lock())
        return;

    if (m_groupData.pNextWindow.lock() == pWindow) { // A -> this -> pWindow -> B >> A -> pWindow -> this -> B
        getGroupPrevious()->m_groupData.pNextWindow = pWindow;
        m_groupData.pNextWindow                     = pWindow->m_groupData.pNextWindow;
        pWindow->m_groupData.pNextWindow            = m_self;

    } else if (pWindow->m_groupData.pNextWindow == m_self) { // A -> pWindow -> this -> B >> A -> this -> pWindow -> B
        pWindow->getGroupPrevious()->m_groupData.pNextWindow = m_self;
        pWindow->m_groupData.pNextWindow                     = m_groupData.pNextWindow;
        m_groupData.pNextWindow                              = pWindow;

    } else { // A -> this -> B | C -> pWindow -> D >> A -> pWindow -> B | C -> this -> D
        std::swap(m_groupData.pNextWindow, pWindow->m_groupData.pNextWindow);
        std::swap(getGroupPrevious()->m_groupData.pNextWindow, pWindow->getGroupPrevious()->m_groupData.pNextWindow);
    }

    std::swap(m_groupData.head, pWindow->m_groupData.head);
    std::swap(m_groupData.locked, pWindow->m_groupData.locked);

    pWindow->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_GROUP | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    pWindow->updateWindowDecos();
}

void CWindow::updateGroupOutputs() {
    if (m_groupData.pNextWindow.expired())
        return;

    PHLWINDOW  curr = m_groupData.pNextWindow.lock();

    const auto WS = m_workspace;

    while (curr.get() != this) {
        curr->m_monitor = m_monitor;
        curr->moveToWorkspace(WS);

        *curr->m_realPosition = m_realPosition->goal();
        *curr->m_realSize     = m_realSize->goal();

        curr = curr->m_groupData.pNextWindow.lock();
    }
}

Vector2D CWindow::middle() {
    return m_realPosition->goal() + m_realSize->goal() / 2.f;
}

bool CWindow::opaque() {
    if (m_alpha->value() != 1.f || m_activeInactiveAlpha->value() != 1.f)
        return false;

    const auto PWORKSPACE = m_workspace;

    if (m_wlSurface->small() && !m_wlSurface->m_fillIgnoreSmall)
        return false;

    if (PWORKSPACE && PWORKSPACE->m_alpha->value() != 1.f)
        return false;

    if (m_isX11 && m_xwaylandSurface && m_xwaylandSurface->m_surface && m_xwaylandSurface->m_surface->m_current.texture)
        return m_xwaylandSurface->m_surface->m_current.texture->m_opaque;

    auto solitaryResource = getSolitaryResource();
    if (!solitaryResource || !solitaryResource->m_current.texture)
        return false;

    // TODO: this is wrong
    const auto EXTENTS = m_xdgSurface->m_surface->m_current.opaque.getExtents();
    if (EXTENTS.w >= m_xdgSurface->m_surface->m_current.bufferSize.x && EXTENTS.h >= m_xdgSurface->m_surface->m_current.bufferSize.y)
        return true;

    return solitaryResource->m_current.texture->m_opaque;
}

float CWindow::rounding() {
    static auto PROUNDING      = CConfigValue<Hyprlang::INT>("decoration:rounding");
    static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

    float       roundingPower = m_ruleApplicator->roundingPower().valueOr(*PROUNDINGPOWER);
    float       rounding      = m_ruleApplicator->rounding().valueOr(*PROUNDING) * (roundingPower / 2.0); /* Make perceived roundness consistent. */

    return rounding;
}

float CWindow::roundingPower() {
    static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

    return m_ruleApplicator->roundingPower().valueOr(std::clamp(*PROUNDINGPOWER, 1.F, 10.F));
}

void CWindow::updateWindowData() {
    const auto PWORKSPACE    = m_workspace;
    const auto WORKSPACERULE = PWORKSPACE ? g_pConfigManager->getWorkspaceRuleFor(PWORKSPACE) : SWorkspaceRule{};
    updateWindowData(WORKSPACERULE);
}

void CWindow::updateWindowData(const SWorkspaceRule& workspaceRule) {
    m_ruleApplicator->borderSize().matchOptional(workspaceRule.borderSize, Desktop::Types::PRIORITY_WORKSPACE_RULE);
    m_ruleApplicator->decorate().matchOptional(workspaceRule.decorate, Desktop::Types::PRIORITY_WORKSPACE_RULE);
    m_ruleApplicator->borderSize().matchOptional(workspaceRule.noBorder ? std::optional<Hyprlang::INT>(0) : std::nullopt, Desktop::Types::PRIORITY_WORKSPACE_RULE);
    m_ruleApplicator->rounding().matchOptional(workspaceRule.noRounding.value_or(false) ? std::optional<Hyprlang::INT>(0) : std::nullopt, Desktop::Types::PRIORITY_WORKSPACE_RULE);
    m_ruleApplicator->noShadow().matchOptional(workspaceRule.noShadow, Desktop::Types::PRIORITY_WORKSPACE_RULE);
}

int CWindow::getRealBorderSize() const {
    if ((m_workspace && isEffectiveInternalFSMode(FSMODE_FULLSCREEN)) || !m_ruleApplicator->decorate().valueOrDefault())
        return 0;

    static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");

    return m_ruleApplicator->borderSize().valueOr(*PBORDERSIZE);
}

float CWindow::getScrollMouse() {
    static auto PINPUTSCROLLFACTOR = CConfigValue<Hyprlang::FLOAT>("input:scroll_factor");
    return m_ruleApplicator->scrollMouse().valueOr(*PINPUTSCROLLFACTOR);
}

float CWindow::getScrollTouchpad() {
    static auto PTOUCHPADSCROLLFACTOR = CConfigValue<Hyprlang::FLOAT>("input:touchpad:scroll_factor");
    return m_ruleApplicator->scrollTouchpad().valueOr(*PTOUCHPADSCROLLFACTOR);
}

bool CWindow::isScrollMouseOverridden() {
    return m_ruleApplicator->scrollMouse().hasValue();
}

bool CWindow::isScrollTouchpadOverridden() {
    return m_ruleApplicator->scrollTouchpad().hasValue();
}

bool CWindow::canBeTorn() {
    static auto PTEARING = CConfigValue<Hyprlang::INT>("general:allow_tearing");
    return m_ruleApplicator->tearing().valueOr(m_tearingHint) && *PTEARING;
}

void CWindow::setSuspended(bool suspend) {
    if (suspend == m_suspended)
        return;

    if (m_isX11 || !m_xdgSurface || !m_xdgSurface->m_toplevel)
        return;

    m_xdgSurface->m_toplevel->setSuspeneded(suspend);
    m_suspended = suspend;
}

bool CWindow::visibleOnMonitor(PHLMONITOR pMonitor) {
    CBox wbox = {m_realPosition->value(), m_realSize->value()};

    if (m_isFloating)
        wbox = getFullWindowBoundingBox();

    return !wbox.intersection({pMonitor->m_position, pMonitor->m_size}).empty();
}

void CWindow::setAnimationsToMove() {
    m_realPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsMove"));
    m_animatingIn = false;
}

void CWindow::onWorkspaceAnimUpdate() {
    // clip box for animated offsets
    if (!m_isFloating || m_pinned || isFullscreen() || m_draggingTiled) {
        m_floatingOffset = Vector2D(0, 0);
        return;
    }

    Vector2D   offset;
    const auto PWORKSPACE = m_workspace;
    if (!PWORKSPACE)
        return;

    const auto PWSMON = m_monitor.lock();
    if (!PWSMON)
        return;

    const auto WINBB = getFullWindowBoundingBox();
    if (PWORKSPACE->m_renderOffset->value().x != 0) {
        const auto PROGRESS = PWORKSPACE->m_renderOffset->value().x / PWSMON->m_size.x;

        if (WINBB.x < PWSMON->m_position.x)
            offset.x += (PWSMON->m_position.x - WINBB.x) * PROGRESS;

        if (WINBB.x + WINBB.width > PWSMON->m_position.x + PWSMON->m_size.x)
            offset.x += (WINBB.x + WINBB.width - PWSMON->m_position.x - PWSMON->m_size.x) * PROGRESS;
    } else if (PWORKSPACE->m_renderOffset->value().y != 0) {
        const auto PROGRESS = PWORKSPACE->m_renderOffset->value().y / PWSMON->m_size.y;

        if (WINBB.y < PWSMON->m_position.y)
            offset.y += (PWSMON->m_position.y - WINBB.y) * PROGRESS;

        if (WINBB.y + WINBB.height > PWSMON->m_position.y + PWSMON->m_size.y)
            offset.y += (WINBB.y + WINBB.height - PWSMON->m_position.y - PWSMON->m_size.y) * PROGRESS;
    }

    m_floatingOffset = offset;
}

void CWindow::onFocusAnimUpdate() {
    // borderangle once
    if (m_borderAngleAnimationProgress->enabled() && !m_borderAngleAnimationProgress->isBeingAnimated()) {
        m_borderAngleAnimationProgress->setValueAndWarp(0.f);
        *m_borderAngleAnimationProgress = 1.f;
    }
}

int CWindow::popupsCount() {
    if (m_isX11 || !m_popupHead)
        return 0;

    int no = -1;
    m_popupHead->breadthfirst([](WP<Desktop::View::CPopup> p, void* d) { *sc<int*>(d) += 1; }, &no);
    return no;
}

int CWindow::surfacesCount() {
    if (m_isX11)
        return 1;

    int no = 0;
    m_wlSurface->resource()->breadthfirst([](SP<CWLSurfaceResource> r, const Vector2D& offset, void* d) { *sc<int*>(d) += 1; }, &no);
    return no;
}

bool CWindow::clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize) {
    const Vector2D REALSIZE = m_realSize->goal();
    const Vector2D MAX      = isFullscreen() ? Vector2D{INFINITY, INFINITY} : maxSize.value_or(Vector2D{INFINITY, INFINITY});
    const Vector2D NEWSIZE  = REALSIZE.clamp(minSize.value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}), MAX);
    const bool     changed  = !(NEWSIZE == REALSIZE);

    if (changed) {
        const Vector2D DELTA = REALSIZE - NEWSIZE;
        *m_realPosition      = m_realPosition->goal() + DELTA / 2.0;
        *m_realSize          = NEWSIZE;
    }

    return changed;
}

bool CWindow::isFullscreen() {
    return m_fullscreenState.internal != FSMODE_NONE;
}

bool CWindow::isEffectiveInternalFSMode(const eFullscreenMode MODE) const {
    return sc<eFullscreenMode>(std::bit_floor(sc<uint8_t>(m_fullscreenState.internal))) == MODE;
}

WORKSPACEID CWindow::workspaceID() {
    return m_workspace ? m_workspace->m_id : m_lastWorkspace;
}

MONITORID CWindow::monitorID() {
    return m_monitor ? m_monitor->m_id : MONITOR_INVALID;
}

bool CWindow::onSpecialWorkspace() {
    return m_workspace ? m_workspace->m_isSpecialWorkspace : g_pCompositor->isWorkspaceSpecial(m_lastWorkspace);
}

std::unordered_map<std::string, std::string> CWindow::getEnv() {

    const auto PID = getPID();

    if (PID <= 1)
        return {};

    std::unordered_map<std::string, std::string> results;

    std::vector<char>                            buffer;
    size_t                                       needle = 0;

#if defined(__linux__)
    //
    std::string   environFile = "/proc/" + std::to_string(PID) + "/environ";
    std::ifstream ifs(environFile, std::ios::binary);

    if (!ifs.good())
        return {};

    buffer.resize(512, '\0');
    while (ifs.read(buffer.data() + needle, 512)) {
        buffer.resize(buffer.size() + 512, '\0');
        needle += 512;
    }
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    int    mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ENV, static_cast<int>(PID)};
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

    static auto PFOCUSONACTIVATE = CConfigValue<Hyprlang::INT>("misc:focus_on_activate");

    m_isUrgent = true;

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "urgent", .data = std::format("{:x}", rc<uintptr_t>(this))});
    EMIT_HOOK_EVENT("urgent", m_self.lock());

    if (!force &&
        (!m_ruleApplicator->focusOnActivate().valueOr(*PFOCUSONACTIVATE) || (m_suppressedEvents & SUPPRESS_ACTIVATE_FOCUSONLY) || (m_suppressedEvents & SUPPRESS_ACTIVATE)))
        return;

    if (!m_isMapped) {
        Log::logger->log(Log::DEBUG, "Ignoring CWindow::activate focus/warp, window is not mapped yet.");
        return;
    }

    if (m_isFloating)
        g_pCompositor->changeWindowZOrder(m_self.lock(), true);

    Desktop::focusState()->fullWindowFocus(m_self.lock());
    warpCursor();
}

void CWindow::onUpdateState() {
    std::optional<bool>      requestsFS = m_xdgSurface ? m_xdgSurface->m_toplevel->m_state.requestsFullscreen : m_xwaylandSurface->m_state.requestsFullscreen;
    std::optional<MONITORID> requestsID = m_xdgSurface ? m_xdgSurface->m_toplevel->m_state.requestsFullscreenMonitor : MONITOR_INVALID;
    std::optional<bool>      requestsMX = m_xdgSurface ? m_xdgSurface->m_toplevel->m_state.requestsMaximize : m_xwaylandSurface->m_state.requestsMaximize;

    if (requestsFS.has_value() && !(m_suppressedEvents & SUPPRESS_FULLSCREEN)) {
        if (requestsID.has_value() && (requestsID.value() != MONITOR_INVALID) && !(m_suppressedEvents & SUPPRESS_FULLSCREEN_OUTPUT)) {
            if (m_isMapped) {
                const auto monitor = g_pCompositor->getMonitorFromID(requestsID.value());
                g_pCompositor->moveWindowToWorkspaceSafe(m_self.lock(), monitor->m_activeWorkspace);
                Desktop::focusState()->rawMonitorFocus(monitor);
            }

            if (!m_isMapped)
                m_wantsInitialFullscreenMonitor = requestsID.value();
        }

        bool fs = requestsFS.value();
        if (m_isMapped)
            g_pCompositor->changeWindowFullscreenModeClient(m_self.lock(), FSMODE_FULLSCREEN, requestsFS.value());

        if (!m_isMapped)
            m_wantsInitialFullscreen = fs;
    }

    if (requestsMX.has_value() && !(m_suppressedEvents & SUPPRESS_MAXIMIZE)) {
        if (m_isMapped) {
            auto window    = m_self.lock();
            auto state     = sc<int8_t>(window->m_fullscreenState.client);
            bool maximized = (state & sc<uint8_t>(FSMODE_MAXIMIZED)) != 0;
            g_pCompositor->changeWindowFullscreenModeClient(window, FSMODE_MAXIMIZED, !maximized);
        }
    }
}

void CWindow::onUpdateMeta() {
    const auto NEWTITLE = fetchTitle();
    bool       doUpdate = false;

    if (m_title != NEWTITLE) {
        m_title = NEWTITLE;
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "windowtitle", .data = std::format("{:x}", rc<uintptr_t>(this))});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "windowtitlev2", .data = std::format("{:x},{}", rc<uintptr_t>(this), m_title)});
        EMIT_HOOK_EVENT("windowTitle", m_self.lock());

        if (m_self == Desktop::focusState()->window()) { // if it's the active, let's post an event to update others
            g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindow", .data = m_class + "," + m_title});
            g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindowv2", .data = std::format("{:x}", rc<uintptr_t>(this))});
            EMIT_HOOK_EVENT("activeWindow", m_self.lock());
        }

        Log::logger->log(Log::DEBUG, "Window {:x} set title to {}", rc<uintptr_t>(this), m_title);
        doUpdate = true;
    }

    const auto NEWCLASS = fetchClass();
    if (m_class != NEWCLASS) {
        m_class = NEWCLASS;

        if (m_self == Desktop::focusState()->window()) { // if it's the active, let's post an event to update others
            g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindow", .data = m_class + "," + m_title});
            g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindowv2", .data = std::format("{:x}", rc<uintptr_t>(this))});
            EMIT_HOOK_EVENT("activeWindow", m_self.lock());
        }

        Log::logger->log(Log::DEBUG, "Window {:x} set class to {}", rc<uintptr_t>(this), m_class);
        doUpdate = true;
    }

    if (doUpdate) {
        m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_TITLE | Desktop::Rule::RULE_PROP_CLASS);
        updateToplevel();
    }
}

std::string CWindow::fetchTitle() {
    if (!m_isX11) {
        if (m_xdgSurface && m_xdgSurface->m_toplevel)
            return m_xdgSurface->m_toplevel->m_state.title;
    } else {
        if (m_xwaylandSurface)
            return m_xwaylandSurface->m_state.title;
    }

    return "";
}

std::string CWindow::fetchClass() {
    if (!m_isX11) {
        if (m_xdgSurface && m_xdgSurface->m_toplevel)
            return m_xdgSurface->m_toplevel->m_state.appid;
    } else {
        if (m_xwaylandSurface)
            return m_xwaylandSurface->m_state.appid;
    }

    return "";
}

void CWindow::onAck(uint32_t serial) {
    const auto SERIAL = std::ranges::find_if(m_pendingSizeAcks | std::views::reverse, [serial](const auto& e) { return e.first <= serial; });

    if (SERIAL == m_pendingSizeAcks.rend())
        return;

    m_pendingSizeAck = *SERIAL;
    std::erase_if(m_pendingSizeAcks, [&](const auto& el) { return el.first <= SERIAL->first; });

    if (m_isX11)
        return;

    m_wlSurface->resource()->m_pending.ackedSize          = m_pendingSizeAck->second; // apply pending size. We pinged, the window ponged.
    m_wlSurface->resource()->m_pending.updated.bits.acked = true;
    m_pendingSizeAck.reset();
}

void CWindow::onResourceChangeX11() {
    if (m_xwaylandSurface->m_surface && !m_wlSurface->resource())
        m_wlSurface->assign(m_xwaylandSurface->m_surface.lock(), m_self.lock());
    else if (!m_xwaylandSurface->m_surface && m_wlSurface->resource())
        m_wlSurface->unassign();

    // update metadata as well,
    // could be first assoc and we need to catch the class
    onUpdateMeta();

    Log::logger->log(Log::DEBUG, "xwayland window {:x} -> association to {:x}", rc<uintptr_t>(m_xwaylandSurface.get()), rc<uintptr_t>(m_wlSurface->resource().get()));
}

void CWindow::onX11ConfigureRequest(CBox box) {

    if (!m_xwaylandSurface->m_surface || !m_xwaylandSurface->m_mapped || !m_isMapped) {
        m_xwaylandSurface->configure(box);
        m_pendingReportedSize = box.size();
        m_reportedSize        = box.size();
        m_reportedPosition    = box.pos();
        updateX11SurfaceScale();
        return;
    }

    g_pHyprRenderer->damageWindow(m_self.lock());

    if (!m_isFloating || isFullscreen() || g_pInputManager->m_currentlyDraggedWindow == m_self) {
        sendWindowSize(true);
        g_pInputManager->refocus();
        g_pHyprRenderer->damageWindow(m_self.lock());
        return;
    }

    if (box.size() > Vector2D{1, 1})
        setHidden(false);
    else
        setHidden(true);

    m_realPosition->setValueAndWarp(xwaylandPositionToReal(box.pos()));
    m_realSize->setValueAndWarp(xwaylandSizeToReal(box.size()));

    m_position = m_realPosition->goal();
    m_size     = m_realSize->goal();

    if (m_pendingReportedSize != box.size() || m_reportedPosition != box.pos()) {
        m_xwaylandSurface->configure(box);
        m_reportedSize        = box.size();
        m_pendingReportedSize = box.size();
        m_reportedPosition    = box.pos();
    }

    updateX11SurfaceScale();
    updateWindowDecos();

    if (!m_workspace || !m_workspace->isVisible())
        return; // further things are only for visible windows

    const auto monitorByRequestedPosition = g_pCompositor->getMonitorFromVector(m_realPosition->goal() + m_realSize->goal() / 2.f);
    const auto currentMonitor             = m_workspace->m_monitor.lock();

    Log::logger->log(
        Log::DEBUG,
        "onX11ConfigureRequest: window '{}' ({:#x}) - workspace '{}' (special={}), currentMonitor='{}', monitorByRequestedPosition='{}', pos={:.0f},{:.0f}, size={:.0f},{:.0f}",
        m_title, (uintptr_t)this, m_workspace->m_name, m_workspace->m_isSpecialWorkspace, currentMonitor ? currentMonitor->m_name : "null",
        monitorByRequestedPosition ? monitorByRequestedPosition->m_name : "null", m_realPosition->goal().x, m_realPosition->goal().y, m_realSize->goal().x, m_realSize->goal().y);

    // Reassign workspace only when moving to a different monitor and not on a special workspace
    // X11 apps send configure requests with positions based on XWayland's monitor layout, such as "0,0",
    // which would incorrectly move windows off special workspaces
    if (monitorByRequestedPosition && monitorByRequestedPosition != currentMonitor && !m_workspace->m_isSpecialWorkspace) {
        Log::logger->log(Log::DEBUG, "onX11ConfigureRequest: reassigning workspace from '{}' to '{}'", m_workspace->m_name, monitorByRequestedPosition->m_activeWorkspace->m_name);
        m_workspace = monitorByRequestedPosition->m_activeWorkspace;
    }

    g_pCompositor->changeWindowZOrder(m_self.lock(), true);

    m_createdOverFullscreen = true;

    g_pHyprRenderer->damageWindow(m_self.lock());
}

void CWindow::warpCursor(bool force) {
    static auto PERSISTENTWARPS        = CConfigValue<Hyprlang::INT>("cursor:persistent_warps");
    const auto  coords                 = m_relativeCursorCoordsOnLastWarp;
    m_relativeCursorCoordsOnLastWarp.x = -1; // reset m_vRelativeCursorCoordsOnLastWarp

    if (*PERSISTENTWARPS && coords.x > 0 && coords.y > 0 && coords < m_size) // don't warp cursor outside the window
        g_pCompositor->warpCursorTo(m_position + coords, force);
    else
        g_pCompositor->warpCursorTo(middle(), force);
}

PHLWINDOW CWindow::getSwallower() {
    static auto PSWALLOWREGEX   = CConfigValue<std::string>("misc:swallow_regex");
    static auto PSWALLOWEXREGEX = CConfigValue<std::string>("misc:swallow_exception_regex");
    static auto PSWALLOW        = CConfigValue<Hyprlang::INT>("misc:enable_swallow");

    if (!*PSWALLOW || std::string{*PSWALLOWREGEX} == STRVAL_EMPTY || (*PSWALLOWREGEX).empty())
        return nullptr;

    // check parent
    std::vector<PHLWINDOW> candidates;
    pid_t                  currentPid = getPID();
    // walk up the tree until we find someone, 25 iterations max.
    for (size_t i = 0; i < 25; ++i) {
        currentPid = getPPIDof(currentPid);

        if (!currentPid)
            break;

        for (auto const& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped || w->isHidden())
                continue;

            if (w->getPID() == currentPid)
                candidates.push_back(w);
        }
    }

    if (!(*PSWALLOWREGEX).empty())
        std::erase_if(candidates, [&](const auto& other) { return !RE2::FullMatch(other->m_class, *PSWALLOWREGEX); });

    if (candidates.empty())
        return nullptr;

    if (!(*PSWALLOWEXREGEX).empty())
        std::erase_if(candidates, [&](const auto& other) { return RE2::FullMatch(other->m_title, *PSWALLOWEXREGEX); });

    if (candidates.empty())
        return nullptr;

    if (candidates.size() == 1)
        return candidates[0];

    // walk up the focus history and find the last focused
    for (auto const& w : Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {
        if (!w)
            continue;

        if (std::ranges::find(candidates.begin(), candidates.end(), w.lock()) != candidates.end())
            return w.lock();
    }

    // if none are found (??) then just return the first one
    return candidates[0];
}

bool CWindow::isX11OverrideRedirect() {
    return m_xwaylandSurface && m_xwaylandSurface->m_overrideRedirect;
}

bool CWindow::isModal() {
    return (m_xwaylandSurface && m_xwaylandSurface->m_modal);
}

Vector2D CWindow::realToReportSize() {
    if (!m_isX11)
        return m_realSize->goal().clamp(Vector2D{0, 0}, Math::VECTOR2D_MAX);

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  REPORTSIZE = m_realSize->goal().clamp(Vector2D{1, 1}, Math::VECTOR2D_MAX);
    const auto  PMONITOR   = m_monitor.lock();

    if (*PXWLFORCESCALEZERO && PMONITOR)
        return REPORTSIZE * PMONITOR->m_scale;

    return REPORTSIZE;
}

Vector2D CWindow::realToReportPosition() {
    if (!m_isX11)
        return m_realPosition->goal();

    return g_pXWaylandManager->waylandToXWaylandCoords(m_realPosition->goal());
}

Vector2D CWindow::xwaylandSizeToReal(Vector2D size) {
    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  PMONITOR = m_monitor.lock();
    const auto  SIZE     = size.clamp(Vector2D{1, 1}, Math::VECTOR2D_MAX);
    const auto  SCALE    = *PXWLFORCESCALEZERO ? PMONITOR->m_scale : 1.0f;

    return SIZE / SCALE;
}

Vector2D CWindow::xwaylandPositionToReal(Vector2D pos) {
    return g_pXWaylandManager->xwaylandToWaylandCoords(pos);
}

void CWindow::updateX11SurfaceScale() {
    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    m_X11SurfaceScaledBy = 1.0f;
    if (m_isX11 && *PXWLFORCESCALEZERO) {
        if (const auto PMONITOR = m_monitor.lock(); PMONITOR)
            m_X11SurfaceScaledBy = PMONITOR->m_scale;
    }
}

void CWindow::sendWindowSize(bool force) {
    const auto PMONITOR = m_monitor.lock();

    Log::logger->log(Log::TRACE, "sendWindowSize: window:{:x},title:{} with real pos {}, real size {} (force: {})", rc<uintptr_t>(this), this->m_title, m_realPosition->goal(),
                     m_realSize->goal(), force);

    // TODO: this should be decoupled from setWindowSize IMO
    const auto REPORTPOS = realToReportPosition();

    const auto REPORTSIZE = realToReportSize();

    if (!force && m_pendingReportedSize == REPORTSIZE && (m_reportedPosition == REPORTPOS || !m_isX11))
        return;

    m_reportedPosition    = REPORTPOS;
    m_pendingReportedSize = REPORTSIZE;
    updateX11SurfaceScale();

    if (m_isX11 && m_xwaylandSurface)
        m_xwaylandSurface->configure({REPORTPOS, REPORTSIZE});
    else if (m_xdgSurface && m_xdgSurface->m_toplevel)
        m_pendingSizeAcks.emplace_back(m_xdgSurface->m_toplevel->setSize(REPORTSIZE), REPORTSIZE.floor());
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
    auto curr = getGroupHead();
    while (curr) {
        if (curr != m_self.lock()) {
            // we don't want to deactivate unfocused xwayland windows
            // because X is weird, keep the behavior for wayland windows
            // also its not really needed for xwayland windows
            // ref: #9760 #9294
            if (!curr->m_isX11 && curr->m_xdgSurface && curr->m_xdgSurface->m_toplevel)
                curr->m_xdgSurface->m_toplevel->setActive(false);
        }

        curr = curr->m_groupData.pNextWindow.lock();
        if (curr == getGroupHead())
            break;
    }
}

bool CWindow::isNotResponding() {
    return g_pANRManager->isNotResponding(m_self.lock());
}

std::optional<std::string> CWindow::xdgTag() {
    if (!m_xdgSurface || !m_xdgSurface->m_toplevel)
        return std::nullopt;

    return m_xdgSurface->m_toplevel->m_toplevelTag;
}

std::optional<std::string> CWindow::xdgDescription() {
    if (!m_xdgSurface || !m_xdgSurface->m_toplevel)
        return std::nullopt;

    return m_xdgSurface->m_toplevel->m_toplevelDescription;
}

PHLWINDOW CWindow::parent() {
    if (m_isX11) {
        auto t = x11TransientFor();

        // don't return a parent that's not mapped
        if (!validMapped(t))
            return nullptr;

        return t;
    }

    if (!m_xdgSurface || !m_xdgSurface->m_toplevel || !m_xdgSurface->m_toplevel->m_parent)
        return nullptr;

    // don't return a parent that's not mapped
    if (!m_xdgSurface->m_toplevel->m_parent->m_window || !validMapped(m_xdgSurface->m_toplevel->m_parent->m_window))
        return nullptr;

    return m_xdgSurface->m_toplevel->m_parent->m_window.lock();
}

bool CWindow::priorityFocus() {
    return !m_isX11 && CAsyncDialogBox::isPriorityDialogBox(getPID());
}

SP<CWLSurfaceResource> CWindow::getSolitaryResource() {
    if (!m_wlSurface || !m_wlSurface->resource())
        return nullptr;

    auto res = m_wlSurface->resource();
    if (m_isX11)
        return res;

    if (popupsCount())
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

Vector2D CWindow::getReportedSize() {
    if (m_isX11)
        return m_reportedSize;
    if (m_wlSurface && m_wlSurface->resource())
        return m_wlSurface->resource()->m_current.ackedSize;
    return m_reportedSize;
}

void CWindow::updateDecorationValues() {
    static auto PACTIVECOL              = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
    static auto PINACTIVECOL            = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");
    static auto PNOGROUPACTIVECOL       = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.nogroup_border_active");
    static auto PNOGROUPINACTIVECOL     = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.nogroup_border");
    static auto PGROUPACTIVECOL         = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_active");
    static auto PGROUPINACTIVECOL       = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_inactive");
    static auto PGROUPACTIVELOCKEDCOL   = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_locked_active");
    static auto PGROUPINACTIVELOCKEDCOL = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_locked_inactive");
    static auto PINACTIVEALPHA          = CConfigValue<Hyprlang::FLOAT>("decoration:inactive_opacity");
    static auto PACTIVEALPHA            = CConfigValue<Hyprlang::FLOAT>("decoration:active_opacity");
    static auto PFULLSCREENALPHA        = CConfigValue<Hyprlang::FLOAT>("decoration:fullscreen_opacity");
    static auto PSHADOWCOL              = CConfigValue<Hyprlang::INT>("decoration:shadow:color");
    static auto PSHADOWCOLINACTIVE      = CConfigValue<Hyprlang::INT>("decoration:shadow:color_inactive");
    static auto PDIMSTRENGTH            = CConfigValue<Hyprlang::FLOAT>("decoration:dim_strength");
    static auto PDIMENABLED             = CConfigValue<Hyprlang::INT>("decoration:dim_inactive");
    static auto PDIMMODAL               = CConfigValue<Hyprlang::INT>("decoration:dim_modal");

    auto* const ACTIVECOL              = sc<CGradientValueData*>((PACTIVECOL.ptr())->getData());
    auto* const INACTIVECOL            = sc<CGradientValueData*>((PINACTIVECOL.ptr())->getData());
    auto* const NOGROUPACTIVECOL       = sc<CGradientValueData*>((PNOGROUPACTIVECOL.ptr())->getData());
    auto* const NOGROUPINACTIVECOL     = sc<CGradientValueData*>((PNOGROUPINACTIVECOL.ptr())->getData());
    auto* const GROUPACTIVECOL         = sc<CGradientValueData*>((PGROUPACTIVECOL.ptr())->getData());
    auto* const GROUPINACTIVECOL       = sc<CGradientValueData*>((PGROUPINACTIVECOL.ptr())->getData());
    auto* const GROUPACTIVELOCKEDCOL   = sc<CGradientValueData*>((PGROUPACTIVELOCKEDCOL.ptr())->getData());
    auto* const GROUPINACTIVELOCKEDCOL = sc<CGradientValueData*>((PGROUPINACTIVELOCKEDCOL.ptr())->getData());

    auto        setBorderColor = [&](CGradientValueData grad) -> void {
        if (grad == m_realBorderColor)
            return;

        m_realBorderColorPrevious = m_realBorderColor;
        m_realBorderColor         = grad;
        m_borderFadeAnimationProgress->setValueAndWarp(0.f);
        *m_borderFadeAnimationProgress = 1.f;
    };

    const bool IS_SHADOWED_BY_MODAL = m_xdgSurface && m_xdgSurface->m_toplevel && m_xdgSurface->m_toplevel->anyChildModal();

    // border
    const auto RENDERDATA = g_pLayoutManager->getCurrentLayout()->requestRenderHints(m_self.lock());
    if (RENDERDATA.isBorderGradient)
        setBorderColor(*RENDERDATA.borderGradient);
    else {
        const bool GROUPLOCKED = m_groupData.pNextWindow.lock() ? getGroupHead()->m_groupData.locked : false;
        if (m_self == Desktop::focusState()->window()) {
            const auto* const ACTIVECOLOR =
                !m_groupData.pNextWindow.lock() ? (!m_groupData.deny ? ACTIVECOL : NOGROUPACTIVECOL) : (GROUPLOCKED ? GROUPACTIVELOCKEDCOL : GROUPACTIVECOL);
            setBorderColor(m_ruleApplicator->activeBorderColor().valueOr(*ACTIVECOLOR));
        } else {
            const auto* const INACTIVECOLOR =
                !m_groupData.pNextWindow.lock() ? (!m_groupData.deny ? INACTIVECOL : NOGROUPINACTIVECOL) : (GROUPLOCKED ? GROUPINACTIVELOCKEDCOL : GROUPINACTIVECOL);
            setBorderColor(m_ruleApplicator->inactiveBorderColor().valueOr(*INACTIVECOLOR));
        }
    }

    // opacity
    const auto PWORKSPACE = m_workspace;
    if (isEffectiveInternalFSMode(FSMODE_FULLSCREEN)) {
        *m_activeInactiveAlpha = m_ruleApplicator->alphaFullscreen().valueOrDefault().applyAlpha(*PFULLSCREENALPHA);
    } else {
        if (m_self == Desktop::focusState()->window())
            *m_activeInactiveAlpha = m_ruleApplicator->alpha().valueOrDefault().applyAlpha(*PACTIVEALPHA);
        else
            *m_activeInactiveAlpha = m_ruleApplicator->alphaInactive().valueOrDefault().applyAlpha(*PINACTIVEALPHA);
    }

    // dim
    float goalDim = 1.F;
    if (m_self == Desktop::focusState()->window() || m_ruleApplicator->noDim().valueOrDefault() || !*PDIMENABLED)
        goalDim = 0;
    else
        goalDim = *PDIMSTRENGTH;

    if (IS_SHADOWED_BY_MODAL && *PDIMMODAL)
        goalDim += (1.F - goalDim) / 2.F;

    *m_dimPercent = goalDim;

    // shadow
    if (!isX11OverrideRedirect() && !m_X11DoesntWantBorders) {
        if (m_self == Desktop::focusState()->window())
            *m_realShadowColor = CHyprColor(*PSHADOWCOL);
        else
            *m_realShadowColor = CHyprColor(*PSHADOWCOLINACTIVE != -1 ? *PSHADOWCOLINACTIVE : *PSHADOWCOL);
    } else
        m_realShadowColor->setValueAndWarp(CHyprColor(0, 0, 0, 0)); // no shadow

    updateWindowDecos();
}

std::optional<double> CWindow::calculateSingleExpr(const std::string& s) {
    const auto        PMONITOR     = m_monitor ? m_monitor : Desktop::focusState()->monitor();
    const auto        CURSOR_LOCAL = g_pInputManager->getMouseCoordsInternal() - (PMONITOR ? PMONITOR->m_position : Vector2D{});

    Math::CExpression expr;
    expr.addVariable("window_w", m_realSize->goal().x);
    expr.addVariable("window_h", m_realSize->goal().y);
    expr.addVariable("window_x", m_realPosition->goal().x - (PMONITOR ? PMONITOR->m_position.x : 0));
    expr.addVariable("window_y", m_realPosition->goal().y - (PMONITOR ? PMONITOR->m_position.y : 0));

    expr.addVariable("monitor_w", PMONITOR ? PMONITOR->m_size.x : 1920);
    expr.addVariable("monitor_h", PMONITOR ? PMONITOR->m_size.y : 1080);

    expr.addVariable("cursor_x", CURSOR_LOCAL.x);
    expr.addVariable("cursor_y", CURSOR_LOCAL.y);

    return expr.compute(s);
}

std::optional<Vector2D> CWindow::calculateExpression(const std::string& s) {
    auto spacePos = s.find(' ');
    if (spacePos == std::string::npos)
        return std::nullopt;

    const auto LHS = calculateSingleExpr(s.substr(0, spacePos));
    const auto RHS = calculateSingleExpr(s.substr(spacePos + 1));

    if (!LHS || !RHS)
        return std::nullopt;

    return Vector2D{*LHS, *RHS};
}

static void setVector2DAnimToMove(WP<CBaseAnimatedVariable> pav) {
    if (!pav)
        return;

    CAnimatedVariable<Vector2D>* animvar = dc<CAnimatedVariable<Vector2D>*>(pav.get());
    animvar->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsMove"));

    if (animvar->m_Context.pWindow)
        animvar->m_Context.pWindow->m_animatingIn = false;
}

void CWindow::mapWindow() {
    static auto PINACTIVEALPHA     = CConfigValue<Hyprlang::FLOAT>("decoration:inactive_opacity");
    static auto PACTIVEALPHA       = CConfigValue<Hyprlang::FLOAT>("decoration:active_opacity");
    static auto PDIMSTRENGTH       = CConfigValue<Hyprlang::FLOAT>("decoration:dim_strength");
    static auto PNEWTAKESOVERFS    = CConfigValue<Hyprlang::INT>("misc:on_focus_under_fullscreen");
    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    const auto  LAST_FOCUS_WINDOW = Desktop::focusState()->window();
    const bool  IS_LAST_IN_FS     = LAST_FOCUS_WINDOW ? LAST_FOCUS_WINDOW->m_fullscreenState.internal != FSMODE_NONE : false;
    const auto  LAST_FS_MODE      = LAST_FOCUS_WINDOW ? LAST_FOCUS_WINDOW->m_fullscreenState.internal : FSMODE_NONE;

    auto        PMONITOR = Desktop::focusState()->monitor();
    if (!Desktop::focusState()->monitor()) {
        Desktop::focusState()->rawMonitorFocus(g_pCompositor->getMonitorFromVector({}));
        PMONITOR = Desktop::focusState()->monitor();
    }
    auto PWORKSPACE = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
    m_monitor       = PMONITOR;
    m_workspace     = PWORKSPACE;
    m_isMapped      = true;
    m_readyToDelete = false;
    m_fadingOut     = false;
    m_title         = fetchTitle();
    m_firstMap      = true;
    m_initialTitle  = m_title;
    m_initialClass  = fetchClass();

    // check for token
    std::string requestedWorkspace = "";
    bool        workspaceSilent    = false;

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

                if (g_pCompositor->getWorkspaceByString(WS.workspace) != m_workspace) {
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

    // checks if the window wants borders and sets the appropriate flag
    g_pXWaylandManager->checkBorders(m_self.lock());

    // registers the animated vars and stuff
    onMap();

    if (g_pXWaylandManager->shouldBeFloated(m_self.lock())) {
        m_isFloating    = true;
        m_requestsFloat = true;
    }

    m_X11ShouldntFocus = m_X11ShouldntFocus || (m_isX11 && isX11OverrideRedirect() && !m_xwaylandSurface->wantsFocus());

    // window rules
    std::optional<eFullscreenMode>                 requestedInternalFSMode, requestedClientFSMode;
    std::optional<Desktop::View::SFullscreenState> requestedFSState;
    if (m_wantsInitialFullscreen || (m_isX11 && m_xwaylandSurface->m_fullscreen))
        requestedClientFSMode = FSMODE_FULLSCREEN;
    MONITORID requestedFSMonitor = m_wantsInitialFullscreenMonitor;

    m_ruleApplicator->readStaticRules();
    {
        if (!m_ruleApplicator->static_.monitor.empty()) {
            const auto& MONITORSTR = m_ruleApplicator->static_.monitor;
            if (MONITORSTR == "unset")
                m_monitor = PMONITOR;
            else {
                const auto MONITOR = g_pCompositor->getMonitorFromString(MONITORSTR);

                if (MONITOR) {
                    m_monitor = MONITOR;

                    const auto PMONITORFROMID = m_monitor.lock();

                    if (m_monitor != PMONITOR) {
                        g_pKeybindManager->m_dispatchers["focusmonitor"](std::to_string(monitorID()));
                        PMONITOR = PMONITORFROMID;
                    }
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

            if (JUSTWORKSPACE == PWORKSPACE->m_name || JUSTWORKSPACE == "name:" + PWORKSPACE->m_name)
                requestedWorkspace = "";

            Log::logger->log(Log::DEBUG, "Rule workspace matched by {}, {} applied.", m_self.lock(), m_ruleApplicator->static_.workspace);
            requestedFSMonitor = MONITOR_INVALID;
        }

        m_isFloating     = m_ruleApplicator->static_.floating.value_or(m_isFloating);
        m_isPseudotiled  = m_ruleApplicator->static_.pseudo.value_or(m_isPseudotiled);
        m_noInitialFocus = m_ruleApplicator->static_.noInitialFocus.value_or(m_noInitialFocus);
        m_pinned         = m_ruleApplicator->static_.pin.value_or(m_pinned);

        if (m_ruleApplicator->static_.fullscreenStateClient || m_ruleApplicator->static_.fullscreenStateInternal) {
            requestedFSState = Desktop::View::SFullscreenState{
                .internal = sc<eFullscreenMode>(m_ruleApplicator->static_.fullscreenStateInternal.value_or(0)),
                .client   = sc<eFullscreenMode>(m_ruleApplicator->static_.fullscreenStateClient.value_or(0)),
            };
        }

        if (!m_ruleApplicator->static_.suppressEvent.empty()) {
            for (const auto& var : m_ruleApplicator->static_.suppressEvent) {
                if (var == "fullscreen")
                    m_suppressedEvents |= Desktop::View::SUPPRESS_FULLSCREEN;
                else if (var == "maximize")
                    m_suppressedEvents |= Desktop::View::SUPPRESS_MAXIMIZE;
                else if (var == "activate")
                    m_suppressedEvents |= Desktop::View::SUPPRESS_ACTIVATE;
                else if (var == "activatefocus")
                    m_suppressedEvents |= Desktop::View::SUPPRESS_ACTIVATE_FOCUSONLY;
                else if (var == "fullscreenoutput")
                    m_suppressedEvents |= Desktop::View::SUPPRESS_FULLSCREEN_OUTPUT;
                else
                    Log::logger->log(Log::ERR, "Error while parsing suppressevent windowrule: unknown event type {}", var);
            }
        }

        if (m_ruleApplicator->static_.fullscreen.value_or(false))
            requestedInternalFSMode = FSMODE_FULLSCREEN;

        if (m_ruleApplicator->static_.maximize.value_or(false))
            requestedInternalFSMode = FSMODE_MAXIMIZED;

        if (!m_ruleApplicator->static_.group.empty()) {
            if (!(m_groupRules & Desktop::View::GROUP_OVERRIDE) && trim(m_ruleApplicator->static_.group) != "group") {
                CVarList2   vars(std::string{m_ruleApplicator->static_.group}, 0, 's');
                std::string vPrev = "";

                for (auto const& v : vars) {
                    if (v == "group")
                        continue;

                    if (v == "set") {
                        m_groupRules |= Desktop::View::GROUP_SET;
                    } else if (v == "new") {
                        // shorthand for `group barred set`
                        m_groupRules |= (Desktop::View::GROUP_SET | Desktop::View::GROUP_BARRED);
                    } else if (v == "lock") {
                        m_groupRules |= Desktop::View::GROUP_LOCK;
                    } else if (v == "invade") {
                        m_groupRules |= Desktop::View::GROUP_INVADE;
                    } else if (v == "barred") {
                        m_groupRules |= Desktop::View::GROUP_BARRED;
                    } else if (v == "deny") {
                        m_groupData.deny = true;
                    } else if (v == "override") {
                        // Clear existing rules
                        m_groupRules = Desktop::View::GROUP_OVERRIDE;
                    } else if (v == "unset") {
                        // Clear existing rules and stop processing
                        m_groupRules = Desktop::View::GROUP_OVERRIDE;
                        break;
                    } else if (v == "always") {
                        if (vPrev == "set" || vPrev == "group")
                            m_groupRules |= Desktop::View::GROUP_SET_ALWAYS;
                        else if (vPrev == "lock")
                            m_groupRules |= Desktop::View::GROUP_LOCK_ALWAYS;
                        else
                            Log::logger->log(Log::ERR, "windowrule `group` does not support `{} always`", vPrev);
                    }
                    vPrev = v;
                }
            }
        }

        if (m_ruleApplicator->static_.content)
            setContentType(sc<NContentType::eContentType>(m_ruleApplicator->static_.content.value()));

        if (m_ruleApplicator->static_.noCloseFor)
            m_closeableSince = Time::steadyNow() + std::chrono::milliseconds(m_ruleApplicator->static_.noCloseFor.value());
    }

    // make it uncloseable if it's a Hyprland dialog
    // TODO: make some closeable?
    if (CAsyncDialogBox::isAsyncDialogBox(getPID()))
        m_closeableSince = Time::steadyNow() + std::chrono::years(10 /* Should be enough, no? */);

    // disallow tiled pinned
    if (m_pinned && !m_isFloating)
        m_pinned = false;

    CVarList2 WORKSPACEARGS = CVarList2(std::move(requestedWorkspace), 0, ' ', false, false);

    if (!WORKSPACEARGS[0].empty()) {
        WORKSPACEID requestedWorkspaceID;
        std::string requestedWorkspaceName;
        if (WORKSPACEARGS.contains("silent"))
            workspaceSilent = true;

        if (WORKSPACEARGS.contains("empty") && PWORKSPACE->getWindows() <= 1) {
            requestedWorkspaceID   = PWORKSPACE->m_id;
            requestedWorkspaceName = PWORKSPACE->m_name;
        } else {
            auto result            = getWorkspaceIDNameFromString(WORKSPACEARGS.join(" ", 0, workspaceSilent ? WORKSPACEARGS.size() - 1 : 0));
            requestedWorkspaceID   = result.id;
            requestedWorkspaceName = result.name;
        }

        if (requestedWorkspaceID != WORKSPACE_INVALID) {
            auto pWorkspace = g_pCompositor->getWorkspaceByID(requestedWorkspaceID);

            if (!pWorkspace)
                pWorkspace = g_pCompositor->createNewWorkspace(requestedWorkspaceID, monitorID(), requestedWorkspaceName, false);

            PWORKSPACE = pWorkspace;

            m_workspace = pWorkspace;
            m_monitor   = pWorkspace->m_monitor;

            if (m_monitor.lock()->m_activeSpecialWorkspace && !pWorkspace->m_isSpecialWorkspace)
                workspaceSilent = true;

            if (!workspaceSilent) {
                if (pWorkspace->m_isSpecialWorkspace)
                    pWorkspace->m_monitor->setSpecialWorkspace(pWorkspace);
                else if (PMONITOR->activeWorkspaceID() != requestedWorkspaceID && !m_noInitialFocus)
                    g_pKeybindManager->m_dispatchers["workspace"](requestedWorkspaceName);

                PMONITOR = Desktop::focusState()->monitor();
            }

            requestedFSMonitor = MONITOR_INVALID;
        } else
            workspaceSilent = false;
    }

    if (m_suppressedEvents & Desktop::View::SUPPRESS_FULLSCREEN_OUTPUT)
        requestedFSMonitor = MONITOR_INVALID;
    else if (requestedFSMonitor != MONITOR_INVALID) {
        if (const auto PM = g_pCompositor->getMonitorFromID(requestedFSMonitor); PM)
            m_monitor = PM;

        const auto PMONITORFROMID = m_monitor.lock();

        if (m_monitor != PMONITOR) {
            g_pKeybindManager->m_dispatchers["focusmonitor"](std::to_string(monitorID()));
            PMONITOR = PMONITORFROMID;
        }
        m_workspace = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
        PWORKSPACE  = m_workspace;

        Log::logger->log(Log::DEBUG, "Requested monitor, applying to {:mw}", m_self.lock());
    }

    if (PWORKSPACE->m_defaultFloating)
        m_isFloating = true;

    if (PWORKSPACE->m_defaultPseudo) {
        m_isPseudotiled      = true;
        CBox desiredGeometry = g_pXWaylandManager->getGeometryForWindow(m_self.lock());
        m_pseudoSize         = Vector2D(desiredGeometry.width, desiredGeometry.height);
    }

    updateWindowData();

    // Verify window swallowing. Get the swallower before calling onWindowCreated(m_self.lock()) because getSwallower() wouldn't get it after if m_self.lock() gets auto grouped.
    const auto SWALLOWER = getSwallower();
    m_swallowed          = SWALLOWER;
    if (m_swallowed)
        m_swallowed->m_currentlySwallowed = true;

    // emit the IPC event before the layout might focus the window to avoid a focus event first
    g_pEventManager->postEvent(SHyprIPCEvent{"openwindow", std::format("{:x},{},{},{}", m_self.lock(), PWORKSPACE->m_name, m_class, m_title)});
    EMIT_HOOK_EVENT("openWindowEarly", m_self.lock());

    if (m_isFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(m_self.lock());
        m_createdOverFullscreen = true;

        if (!m_ruleApplicator->static_.size.empty()) {
            const auto COMPUTED = calculateExpression(m_ruleApplicator->static_.size);
            if (!COMPUTED)
                Log::logger->log(Log::ERR, "failed to parse {} as an expression", m_ruleApplicator->static_.size);
            else {
                *m_realSize = *COMPUTED;
                setHidden(false);
            }
        }

        if (!m_ruleApplicator->static_.position.empty()) {
            const auto COMPUTED = calculateExpression(m_ruleApplicator->static_.position);
            if (!COMPUTED)
                Log::logger->log(Log::ERR, "failed to parse {} as an expression", m_ruleApplicator->static_.position);
            else {
                *m_realPosition = *COMPUTED + PMONITOR->m_position;
                setHidden(false);
            }
        }

        if (m_ruleApplicator->static_.center.value_or(false)) {
            const auto WORKAREA = PMONITOR->logicalBoxMinusReserved();
            *m_realPosition     = WORKAREA.middle() - m_realSize->goal() / 2.f;
        }

        // set the pseudo size to the GOAL of our current size
        // because the windows are animated on RealSize
        m_pseudoSize = m_realSize->goal();

        g_pCompositor->changeWindowZOrder(m_self.lock(), true);
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(m_self.lock());

        bool setPseudo = false;

        if (!m_ruleApplicator->static_.size.empty()) {
            const auto COMPUTED = calculateExpression(m_ruleApplicator->static_.size);
            if (!COMPUTED)
                Log::logger->log(Log::ERR, "failed to parse {} as an expression", m_ruleApplicator->static_.size);
            else {
                setPseudo    = true;
                m_pseudoSize = *COMPUTED;
                setHidden(false);
            }
        }

        if (!setPseudo)
            m_pseudoSize = m_realSize->goal() - Vector2D(10, 10);
    }

    const auto PFOCUSEDWINDOWPREV = Desktop::focusState()->window();

    if (m_ruleApplicator->allowsInput().valueOrDefault()) { // if default value wasn't set to false getPriority() would throw an exception
        m_ruleApplicator->noFocusOverride(Desktop::Types::COverridableVar(false, m_ruleApplicator->allowsInput().getPriority()));
        m_noInitialFocus   = false;
        m_X11ShouldntFocus = false;
    }

    // check LS focus grab
    const auto PFORCEFOCUS  = g_pCompositor->getForceFocus();
    const auto PLSFROMFOCUS = g_pCompositor->getLayerSurfaceFromSurface(Desktop::focusState()->surface());
    if (PLSFROMFOCUS && PLSFROMFOCUS->m_layerSurface->m_current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        m_noInitialFocus = true;

    if (m_workspace->m_hasFullscreenWindow && !requestedInternalFSMode.has_value() && !requestedClientFSMode.has_value() && !m_isFloating) {
        if (*PNEWTAKESOVERFS == 0)
            m_noInitialFocus = true;
        else if (*PNEWTAKESOVERFS == 1)
            requestedInternalFSMode = m_workspace->m_fullscreenMode;
        else if (*PNEWTAKESOVERFS == 2)
            g_pCompositor->setWindowFullscreenInternal(m_workspace->getFullscreenWindow(), FSMODE_NONE);
    }

    if (!m_ruleApplicator->noFocus().valueOrDefault() && !m_noInitialFocus && (!isX11OverrideRedirect() || (m_isX11 && m_xwaylandSurface->wantsFocus())) && !workspaceSilent &&
        (!PFORCEFOCUS || PFORCEFOCUS == m_self.lock()) && !g_pInputManager->isConstrained()) {

        // this window should gain focus: if it's grouped, preserve fullscreen state.
        const bool SAME_GROUP = hasInGroup(LAST_FOCUS_WINDOW);

        if (IS_LAST_IN_FS && SAME_GROUP) {
            Desktop::focusState()->rawWindowFocus(m_self.lock());
            g_pCompositor->setWindowFullscreenInternal(m_self.lock(), LAST_FS_MODE);
        } else
            Desktop::focusState()->fullWindowFocus(m_self.lock());

        m_activeInactiveAlpha->setValueAndWarp(*PACTIVEALPHA);
        m_dimPercent->setValueAndWarp(m_ruleApplicator->noDim().valueOrDefault() ? 0.f : *PDIMSTRENGTH);
    } else {
        m_activeInactiveAlpha->setValueAndWarp(*PINACTIVEALPHA);
        m_dimPercent->setValueAndWarp(0);
    }

    if (requestedClientFSMode.has_value() && (m_suppressedEvents & Desktop::View::SUPPRESS_FULLSCREEN))
        requestedClientFSMode = sc<eFullscreenMode>(sc<uint8_t>(requestedClientFSMode.value_or(FSMODE_NONE)) & ~sc<uint8_t>(FSMODE_FULLSCREEN));
    if (requestedClientFSMode.has_value() && (m_suppressedEvents & Desktop::View::SUPPRESS_MAXIMIZE))
        requestedClientFSMode = sc<eFullscreenMode>(sc<uint8_t>(requestedClientFSMode.value_or(FSMODE_NONE)) & ~sc<uint8_t>(FSMODE_MAXIMIZED));

    if (!m_noInitialFocus && (requestedInternalFSMode.has_value() || requestedClientFSMode.has_value() || requestedFSState.has_value())) {
        // fix fullscreen on requested (basically do a switcheroo)
        if (m_workspace->m_hasFullscreenWindow)
            g_pCompositor->setWindowFullscreenInternal(m_workspace->getFullscreenWindow(), FSMODE_NONE);

        m_realPosition->warp();
        m_realSize->warp();
        if (requestedFSState.has_value()) {
            m_ruleApplicator->syncFullscreenOverride(Desktop::Types::COverridableVar(false, Desktop::Types::PRIORITY_WINDOW_RULE));
            g_pCompositor->setWindowFullscreenState(m_self.lock(), requestedFSState.value());
        } else if (requestedInternalFSMode.has_value() && requestedClientFSMode.has_value() && !m_ruleApplicator->syncFullscreen().valueOrDefault())
            g_pCompositor->setWindowFullscreenState(m_self.lock(),
                                                    Desktop::View::SFullscreenState{.internal = requestedInternalFSMode.value(), .client = requestedClientFSMode.value()});
        else if (requestedInternalFSMode.has_value())
            g_pCompositor->setWindowFullscreenInternal(m_self.lock(), requestedInternalFSMode.value());
        else if (requestedClientFSMode.has_value())
            g_pCompositor->setWindowFullscreenClient(m_self.lock(), requestedClientFSMode.value());
    }

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    updateToplevel();
    m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_ALL);

    if (workspaceSilent) {
        if (validMapped(PFOCUSEDWINDOWPREV)) {
            Desktop::focusState()->rawWindowFocus(PFOCUSEDWINDOWPREV);
            PFOCUSEDWINDOWPREV->updateWindowDecos(); // need to for some reason i cba to find out why
        } else if (!PFOCUSEDWINDOWPREV)
            Desktop::focusState()->rawWindowFocus(nullptr);
    }

    // swallow
    if (SWALLOWER) {
        g_pLayoutManager->getCurrentLayout()->onWindowRemoved(SWALLOWER);
        g_pHyprRenderer->damageWindow(SWALLOWER);
        SWALLOWER->setHidden(true);
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
    }

    m_firstMap = false;

    Log::logger->log(Log::DEBUG, "Map request dispatched, monitor {}, window pos: {:5j}, window size: {:5j}", PMONITOR->m_name, m_realPosition->goal(), m_realSize->goal());

    // emit the hook event here after basic stuff has been initialized
    EMIT_HOOK_EVENT("openWindow", m_self.lock());

    // apply data from default decos. Borders, shadows.
    g_pDecorationPositioner->forceRecalcFor(m_self.lock());
    updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(m_self.lock());

    // do animations
    g_pDesktopAnimationManager->startAnimation(m_self.lock(), CDesktopAnimationManager::ANIMATION_TYPE_IN);

    m_realPosition->setCallbackOnEnd(setVector2DAnimToMove);
    m_realSize->setCallbackOnEnd(setVector2DAnimToMove);

    // recalc the values for this window
    updateDecorationValues();
    // avoid this window being visible
    if (PWORKSPACE->m_hasFullscreenWindow && !isFullscreen() && !m_isFloating)
        m_alpha->setValueAndWarp(0.f);

    g_pCompositor->setPreferredScaleForSurface(wlSurface()->resource(), PMONITOR->m_scale);
    g_pCompositor->setPreferredTransformForSurface(wlSurface()->resource(), PMONITOR->m_transform);

    if (g_pSeatManager->m_mouse.expired() || !g_pInputManager->isConstrained())
        g_pInputManager->sendMotionEventsToFocused();

    // fix some xwayland apps that don't behave nicely
    m_reportedSize = m_pendingReportedSize;

    if (m_workspace)
        m_workspace->updateWindows();

    if (PMONITOR && isX11OverrideRedirect())
        m_X11SurfaceScaledBy = PMONITOR->m_scale;
}

void CWindow::unmapWindow() {
    Log::logger->log(Log::DEBUG, "{:c} unmapped", m_self.lock());

    static auto PEXITRETAINSFS = CConfigValue<Hyprlang::INT>("misc:exit_window_retains_fullscreen");

    const auto  CURRENTWINDOWFSSTATE = isFullscreen();
    const auto  CURRENTFSMODE        = m_fullscreenState.internal;

    if (!wlSurface()->exists() || !m_isMapped) {
        Log::logger->log(Log::WARN, "{} unmapped without being mapped??", m_self.lock());
        m_fadingOut = false;
        return;
    }

    const auto PMONITOR = m_monitor.lock();
    if (PMONITOR) {
        m_originalClosedPos     = m_realPosition->value() - PMONITOR->m_position;
        m_originalClosedSize    = m_realSize->value();
        m_originalClosedExtents = getFullWindowExtents();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"closewindow", std::format("{:x}", m_self.lock())});
    EMIT_HOOK_EVENT("closeWindow", m_self.lock());

    if (m_isFloating && !m_isX11 && m_ruleApplicator->persistentSize().valueOrDefault()) {
        Log::logger->log(Log::DEBUG, "storing floating size {}x{} for window {}::{} on close", m_realSize->value().x, m_realSize->value().y, m_class, m_title);
        g_pConfigManager->storeFloatingSize(m_self.lock(), m_realSize->value());
    }

    if (isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(m_self.lock(), FSMODE_NONE);

    // Allow the renderer to catch the last frame.
    if (g_pHyprRenderer->shouldRenderWindow(m_self.lock()))
        g_pHyprRenderer->makeSnapshot(m_self.lock());

    // swallowing
    if (valid(m_swallowed)) {
        if (m_swallowed->m_currentlySwallowed) {
            m_swallowed->m_currentlySwallowed = false;
            m_swallowed->setHidden(false);

            if (m_groupData.pNextWindow.lock())
                m_swallowed->m_groupSwallowed = true; // flag for the swallowed window to be created into the group where it belongs when auto_group = false.

            g_pLayoutManager->getCurrentLayout()->onWindowCreated(m_swallowed.lock());
        }

        m_swallowed->m_groupSwallowed = false;
        m_swallowed.reset();
    }

    bool      wasLastWindow = false;
    PHLWINDOW nextInGroup   = [this] -> PHLWINDOW {
        if (!m_groupData.pNextWindow)
            return nullptr;

        // walk the history to find a suitable window
        const auto HISTORY = Desktop::History::windowTracker()->fullHistory();
        for (const auto& w : HISTORY | std::views::reverse) {
            if (!w || !w->m_isMapped || w == m_self)
                continue;

            if (!hasInGroup(w.lock()))
                continue;

            return w.lock();
        }

        return nullptr;
    }();

    if (m_self.lock() == Desktop::focusState()->window()) {
        wasLastWindow = true;
        Desktop::focusState()->resetWindowFocus();

        g_pInputManager->releaseAllMouseButtons();
    }

    if (m_self.lock() == g_pInputManager->m_currentlyDraggedWindow.lock())
        CKeybindManager::changeMouseBindMode(MBIND_INVALID);

    // remove the fullscreen window status from workspace if we closed it
    const auto PWORKSPACE = m_workspace;

    if (PWORKSPACE->m_hasFullscreenWindow && isFullscreen())
        PWORKSPACE->m_hasFullscreenWindow = false;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(m_self.lock());

    g_pHyprRenderer->damageWindow(m_self.lock());

    // do this after onWindowRemoved because otherwise it'll think the window is invalid
    m_isMapped = false;

    // refocus on a new window if needed
    if (wasLastWindow) {
        static auto FOCUSONCLOSE = CConfigValue<Hyprlang::INT>("input:focus_on_close");
        PHLWINDOW   candidate    = nextInGroup;

        if (!candidate) {
            if (*FOCUSONCLOSE)
                candidate = (g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(),
                                                                  Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING));
            else
                candidate = g_pLayoutManager->getCurrentLayout()->getNextWindowCandidate(m_self.lock());
        }

        Log::logger->log(Log::DEBUG, "On closed window, new focused candidate is {}", candidate);

        if (candidate != Desktop::focusState()->window() && candidate) {
            if (candidate == nextInGroup)
                Desktop::focusState()->rawWindowFocus(candidate);
            else
                Desktop::focusState()->fullWindowFocus(candidate);

            if ((*PEXITRETAINSFS || candidate == nextInGroup) && CURRENTWINDOWFSSTATE)
                g_pCompositor->setWindowFullscreenInternal(candidate, CURRENTFSMODE);
        }

        if (!candidate && m_workspace && m_workspace->getWindows() == 0)
            g_pInputManager->refocus();

        g_pInputManager->sendMotionEventsToFocused();

        // CWindow::onUnmap will remove this window's active status, but we can't really do it above.
        if (m_self.lock() == Desktop::focusState()->window() || !Desktop::focusState()->window()) {
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","});
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ""});
            EMIT_HOOK_EVENT("activeWindow", PHLWINDOW{nullptr});
        }
    } else {
        Log::logger->log(Log::DEBUG, "Unmapped was not focused, ignoring a refocus.");
    }

    m_fadingOut = true;

    g_pCompositor->addToFadingOutSafe(m_self.lock());

    if (!m_X11DoesntWantBorders)                                            // don't animate out if they weren't animated in.
        *m_realPosition = m_realPosition->value() + Vector2D(0.01f, 0.01f); // it has to be animated, otherwise CesktopAnimationManager will ignore it

    // anims
    g_pDesktopAnimationManager->startAnimation(m_self.lock(), CDesktopAnimationManager::ANIMATION_TYPE_OUT);

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    // force report all sizes (QT sometimes has an issue with this)
    if (m_workspace)
        m_workspace->forceReportSizesToWindows();

    // update lastwindow after focus
    onUnmap();
}

void CWindow::commitWindow() {
    if (!m_isX11 && m_xdgSurface->m_initialCommit) {
        // try to calculate static rules already for any floats
        m_ruleApplicator->readStaticRules(true);

        Vector2D predSize = g_pLayoutManager->getCurrentLayout()->predictSizeForNewWindow(m_self.lock());

        Log::logger->log(Log::DEBUG, "Layout predicts size {} for {}", predSize, m_self.lock());

        m_xdgSurface->m_toplevel->setSize(predSize);
        return;
    }

    if (!m_isMapped || isHidden())
        return;

    if (m_isX11)
        m_reportedSize = m_pendingReportedSize;

    if (!m_isX11 && !isFullscreen() && m_isFloating) {
        const auto MINSIZE = m_xdgSurface->m_toplevel->layoutMinSize();
        const auto MAXSIZE = m_xdgSurface->m_toplevel->layoutMaxSize();

        if (clampWindowSize(MINSIZE, MAXSIZE > Vector2D{1, 1} ? std::optional<Vector2D>{MAXSIZE} : std::nullopt))
            g_pHyprRenderer->damageWindow(m_self.lock());
    }

    if (!m_workspace->m_visible)
        return;

    const auto PMONITOR = m_monitor.lock();

    g_pHyprRenderer->damageSurface(wlSurface()->resource(), m_realPosition->goal().x, m_realPosition->goal().y, m_isX11 ? 1.0 / m_X11SurfaceScaledBy : 1.0);

    if (!m_isX11) {
        m_subsurfaceHead->recheckDamageForSubsurfaces();
        m_popupHead->recheckTree();
    }

    // tearing: if solitary, redraw it. This still might be a single surface window
    if (PMONITOR && PMONITOR->m_solitaryClient.lock() == m_self.lock() && canBeTorn() && PMONITOR->m_tearingState.canTear && wlSurface()->resource()->m_current.texture) {
        CRegion damageBox{wlSurface()->resource()->m_current.accumulateBufferDamage()};

        if (!damageBox.empty()) {
            if (PMONITOR->m_tearingState.busy) {
                PMONITOR->m_tearingState.frameScheduledWhileBusy = true;
            } else {
                PMONITOR->m_tearingState.nextRenderTorn = true;
                g_pHyprRenderer->renderMonitor(PMONITOR);
            }
        }
    }
}

void CWindow::destroyWindow() {
    Log::logger->log(Log::DEBUG, "{:c} destroyed, queueing.", m_self.lock());

    if (m_self.lock() == Desktop::focusState()->window()) {
        Desktop::focusState()->window().reset();
        Desktop::focusState()->surface().reset();
    }

    wlSurface()->unassign();

    m_listeners = {};

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(m_self.lock());

    m_readyToDelete = true;

    m_xdgSurface.reset();

    m_listeners.unmap.reset();
    m_listeners.destroy.reset();
    m_listeners.map.reset();
    m_listeners.commit.reset();

    if (!m_fadingOut) {
        Log::logger->log(Log::DEBUG, "Unmapped {} removed instantly", m_self.lock());
        g_pCompositor->removeWindowFromVectorSafe(m_self.lock()); // most likely X11 unmanaged or sumn
    }
}

void CWindow::activateX11() {
    Log::logger->log(Log::DEBUG, "X11 Activate request for window {}", m_self.lock());

    if (isX11OverrideRedirect()) {

        Log::logger->log(Log::DEBUG, "Unmanaged X11 {} requests activate", m_self.lock());

        if (Desktop::focusState()->window() && Desktop::focusState()->window()->getPID() != getPID())
            return;

        if (!m_xwaylandSurface->wantsFocus())
            return;

        Desktop::focusState()->fullWindowFocus(m_self.lock());
        return;
    }

    if (m_self.lock() == Desktop::focusState()->window() || (m_suppressedEvents & Desktop::View::SUPPRESS_ACTIVATE))
        return;

    activate();
}

void CWindow::unmanagedSetGeometry() {
    if (!m_isMapped || !m_xwaylandSurface || !m_xwaylandSurface->m_overrideRedirect)
        return;

    const auto POS = m_realPosition->goal();
    const auto SIZ = m_realSize->goal();

    if (m_xwaylandSurface->m_geometry.size() > Vector2D{1, 1})
        setHidden(false);
    else
        setHidden(true);

    if (isFullscreen() || !m_isFloating) {
        sendWindowSize(true);
        g_pHyprRenderer->damageWindow(m_self.lock());
        return;
    }

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  LOGICALPOS = g_pXWaylandManager->xwaylandToWaylandCoords(m_xwaylandSurface->m_geometry.pos());

    if (abs(std::floor(POS.x) - LOGICALPOS.x) > 2 || abs(std::floor(POS.y) - LOGICALPOS.y) > 2 || abs(std::floor(SIZ.x) - m_xwaylandSurface->m_geometry.width) > 2 ||
        abs(std::floor(SIZ.y) - m_xwaylandSurface->m_geometry.height) > 2) {
        Log::logger->log(Log::DEBUG, "Unmanaged window {} requests geometry update to {:j} {:j}", m_self.lock(), LOGICALPOS, m_xwaylandSurface->m_geometry.size());

        g_pHyprRenderer->damageWindow(m_self.lock());
        m_realPosition->setValueAndWarp(Vector2D(LOGICALPOS.x, LOGICALPOS.y));

        if (abs(std::floor(SIZ.x) - m_xwaylandSurface->m_geometry.w) > 2 || abs(std::floor(SIZ.y) - m_xwaylandSurface->m_geometry.h) > 2)
            m_realSize->setValueAndWarp(m_xwaylandSurface->m_geometry.size());

        if (*PXWLFORCESCALEZERO) {
            if (const auto PMONITOR = m_monitor.lock(); PMONITOR) {
                m_realSize->setValueAndWarp(m_realSize->goal() / PMONITOR->m_scale);
            }
        }

        m_position = m_realPosition->goal();
        m_size     = m_realSize->goal();

        m_workspace = g_pCompositor->getMonitorFromVector(m_realPosition->value() + m_realSize->value() / 2.f)->m_activeWorkspace;

        g_pCompositor->changeWindowZOrder(m_self.lock(), true);
        updateWindowDecos();
        g_pHyprRenderer->damageWindow(m_self.lock());

        m_reportedPosition    = m_realPosition->goal();
        m_pendingReportedSize = m_realSize->goal();
    }
}

std::optional<Vector2D> CWindow::minSize() {
    // first check for overrides
    if (m_ruleApplicator->minSize().hasValue())
        return m_ruleApplicator->minSize().value();

    // then check if we have any proto overrides
    bool hasSizeHints = m_xwaylandSurface ? m_xwaylandSurface->m_sizeHints : false;
    bool hasTopLevel  = m_xdgSurface ? m_xdgSurface->m_toplevel : false;
    if ((m_isX11 && !hasSizeHints) || (!m_isX11 && !hasTopLevel))
        return std::nullopt;

    Vector2D minSize = m_isX11 ? Vector2D(m_xwaylandSurface->m_sizeHints->min_width, m_xwaylandSurface->m_sizeHints->min_height) : m_xdgSurface->m_toplevel->layoutMinSize();

    minSize = minSize.clamp({1, 1});

    return minSize;
}

std::optional<Vector2D> CWindow::maxSize() {
    // first check for overrides
    if (m_ruleApplicator->maxSize().hasValue())
        return m_ruleApplicator->maxSize().value();

    // then check if we have any proto overrides
    if (((m_isX11 && !m_xwaylandSurface->m_sizeHints) || (!m_isX11 && (!m_xdgSurface || !m_xdgSurface->m_toplevel)) || m_ruleApplicator->noMaxSize().valueOrDefault()))
        return std::nullopt;

    constexpr const double NO_MAX_SIZE_LIMIT = std::numeric_limits<double>::max();

    Vector2D maxSize = m_isX11 ? Vector2D(m_xwaylandSurface->m_sizeHints->max_width, m_xwaylandSurface->m_sizeHints->max_height) : m_xdgSurface->m_toplevel->layoutMaxSize();

    if (maxSize.x < 5)
        maxSize.x = NO_MAX_SIZE_LIMIT;
    if (maxSize.y < 5)
        maxSize.y = NO_MAX_SIZE_LIMIT;

    return maxSize;
}
