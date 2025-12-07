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
#include <string_view>
#include "Window.hpp"
#include "state/FocusState.hpp"
#include "../Compositor.hpp"
#include "../render/decorations/CHyprDropShadowDecoration.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../render/decorations/CHyprBorderDecoration.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../managers/TokenManager.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../managers/ANRManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/core/Subcompositor.hpp"
#include "../protocols/ContentType.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../xwayland/XWayland.hpp"
#include "../helpers/Color.hpp"
#include "../helpers/math/Expression.hpp"
#include "../events/Events.hpp"
#include "../managers/XWaylandManager.hpp"
#include "../render/Renderer.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/input/InputManager.hpp"

#include <hyprutils/string/String.hpp>

using namespace Hyprutils::String;
using namespace Hyprutils::Animation;
using enum NContentType::eContentType;

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

    pWindow->m_wlSurface->assign(pWindow->m_xdgSurface->m_surface.lock(), pWindow);

    return pWindow;
}

CWindow::CWindow(SP<CXDGSurfaceResource> resource) : m_xdgSurface(resource) {
    m_wlSurface = CWLSurface::create();

    m_listeners.map            = m_xdgSurface->m_events.map.listen([this] { Events::listener_mapWindow(this, nullptr); });
    m_listeners.ack            = m_xdgSurface->m_events.ack.listen([this](uint32_t d) { onAck(d); });
    m_listeners.unmap          = m_xdgSurface->m_events.unmap.listen([this] { Events::listener_unmapWindow(this, nullptr); });
    m_listeners.destroy        = m_xdgSurface->m_events.destroy.listen([this] { Events::listener_destroyWindow(this, nullptr); });
    m_listeners.commit         = m_xdgSurface->m_events.commit.listen([this] { Events::listener_commitWindow(this, nullptr); });
    m_listeners.updateState    = m_xdgSurface->m_toplevel->m_events.stateChanged.listen([this] { onUpdateState(); });
    m_listeners.updateMetadata = m_xdgSurface->m_toplevel->m_events.metadataChanged.listen([this] { onUpdateMeta(); });
}

CWindow::CWindow(SP<CXWaylandSurface> surface) : m_xwaylandSurface(surface) {
    m_wlSurface = CWLSurface::create();

    m_listeners.map              = m_xwaylandSurface->m_events.map.listen([this] { Events::listener_mapWindow(this, nullptr); });
    m_listeners.unmap            = m_xwaylandSurface->m_events.unmap.listen([this] { Events::listener_unmapWindow(this, nullptr); });
    m_listeners.destroy          = m_xwaylandSurface->m_events.destroy.listen([this] { Events::listener_destroyWindow(this, nullptr); });
    m_listeners.commit           = m_xwaylandSurface->m_events.commit.listen([this] { Events::listener_commitWindow(this, nullptr); });
    m_listeners.configureRequest = m_xwaylandSurface->m_events.configureRequest.listen([this](const CBox& box) { onX11ConfigureRequest(box); });
    m_listeners.updateState      = m_xwaylandSurface->m_events.stateChanged.listen([this] { onUpdateState(); });
    m_listeners.updateMetadata   = m_xwaylandSurface->m_events.metadataChanged.listen([this] { onUpdateMeta(); });
    m_listeners.resourceChange   = m_xwaylandSurface->m_events.resourceChange.listen([this] { onResourceChangeX11(); });
    m_listeners.activate         = m_xwaylandSurface->m_events.activate.listen([this] { Events::listener_activateX11(this, nullptr); });

    if (m_xwaylandSurface->m_overrideRedirect)
        m_listeners.setGeometry = m_xwaylandSurface->m_events.setGeometry.listen([this] { Events::listener_unmanagedSetGeometry(this, nullptr); });
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

SBoxExtents CWindow::getFullWindowExtents() {
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
            [](WP<CPopup> popup, void* data) {
                if (!popup->m_wlSurface || !popup->m_wlSurface->resource())
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

CBox CWindow::getFullWindowBoundingBox() {
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

    if (!PMONITOR)
        return {m_position, m_size};

    auto POS  = m_position;
    auto SIZE = m_size;

    if (isFullscreen()) {
        POS  = PMONITOR->m_position;
        SIZE = PMONITOR->m_size;

        return CBox{sc<int>(POS.x), sc<int>(POS.y), sc<int>(SIZE.x), sc<int>(SIZE.y)};
    }

    if (DELTALESSTHAN(POS.y - PMONITOR->m_position.y, PMONITOR->m_reservedArea.top(), 1)) {
        POS.y = PMONITOR->m_position.y;
        SIZE.y += PMONITOR->m_reservedArea.top();
    }
    if (DELTALESSTHAN(POS.x - PMONITOR->m_position.x, PMONITOR->m_reservedArea.left(), 1)) {
        POS.x = PMONITOR->m_position.x;
        SIZE.x += PMONITOR->m_reservedArea.left();
    }
    if (DELTALESSTHAN(POS.x + SIZE.x - PMONITOR->m_position.x, PMONITOR->m_size.x - PMONITOR->m_reservedArea.right(), 1)) {
        SIZE.x += PMONITOR->m_reservedArea.right();
    }
    if (DELTALESSTHAN(POS.y + SIZE.y - PMONITOR->m_position.y, PMONITOR->m_size.y - PMONITOR->m_reservedArea.bottom(), 1)) {
        SIZE.y += PMONITOR->m_reservedArea.bottom();
    }

    return CBox{sc<int>(POS.x), sc<int>(POS.y), sc<int>(SIZE.x), sc<int>(SIZE.y)};
}

SBoxExtents CWindow::getWindowExtentsUnified(uint64_t properties) {
    SBoxExtents extents = {.topLeft = {0, 0}, .bottomRight = {0, 0}};
    if (properties & RESERVED_EXTENTS)
        extents.addExtents(g_pDecorationPositioner->getWindowDecorationReserved(m_self));
    if (properties & INPUT_EXTENTS)
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
                SInitialWorkspaceToken token = std::any_cast<SInitialWorkspaceToken>(TOKEN->m_data);
                if (token.primaryOwner == m_self) {
                    token.workspace = pWorkspace->getConfigName();
                    TOKEN->m_data   = token;
                }
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
                SInitialWorkspaceToken token = std::any_cast<SInitialWorkspaceToken>(TOKEN->m_data);
                if (token.primaryOwner == m_self)
                    g_pTokenManager->removeToken(TOKEN);
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
    const auto PAV = pav.lock();
    if (!PAV)
        return;

    if (PAV->getStyle() != "loop" || !PAV->enabled())
        return;

    const auto PANIMVAR = dc<CAnimatedVariable<float>*>(PAV.get());

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

    return popup && popup->m_wlSurface->resource();
}

void CWindow::applyGroupRules() {
    if ((m_groupRules & GROUP_SET && m_firstMap) || m_groupRules & GROUP_SET_ALWAYS)
        createGroup();

    if (m_groupData.pNextWindow.lock() && ((m_groupRules & GROUP_LOCK && m_firstMap) || m_groupRules & GROUP_LOCK_ALWAYS))
        getGroupHead()->m_groupData.locked = true;
}

void CWindow::createGroup() {
    if (m_groupData.deny) {
        Debug::log(LOG, "createGroup: window:{:x},title:{} is denied as a group, ignored", rc<uintptr_t>(this), this->m_title);
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
            Debug::log(LOG, "destoryGroup: window:{:x},title:{} has rule [group set always], ignored", rc<uintptr_t>(this), this->m_title);
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

int CWindow::getRealBorderSize() {
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
    m_popupHead->breadthfirst([](WP<CPopup> p, void* d) { *sc<int*>(d) += 1; }, &no);
    return no;
}

int CWindow::surfacesCount() {
    if (m_isX11)
        return 1;

    int no = 0;
    m_wlSurface->resource()->breadthfirst([](SP<CWLSurfaceResource> r, const Vector2D& offset, void* d) { *sc<int*>(d) += 1; }, &no);
    return no;
}

void CWindow::clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize) {
    const Vector2D REALSIZE = m_realSize->goal();
    const Vector2D MAX      = isFullscreen() ? Vector2D{INFINITY, INFINITY} : maxSize.value_or(Vector2D{INFINITY, INFINITY});
    const Vector2D NEWSIZE  = REALSIZE.clamp(minSize.value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}), MAX);
    const Vector2D DELTA    = REALSIZE - NEWSIZE;

    *m_realPosition = m_realPosition->goal() + DELTA / 2.0;
    *m_realSize     = NEWSIZE;
}

bool CWindow::isFullscreen() {
    return m_fullscreenState.internal != FSMODE_NONE;
}

bool CWindow::isEffectiveInternalFSMode(const eFullscreenMode MODE) {
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
        Debug::log(LOG, "Ignoring CWindow::activate focus/warp, window is not mapped yet.");
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

        Debug::log(LOG, "Window {:x} set title to {}", rc<uintptr_t>(this), m_title);
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

        Debug::log(LOG, "Window {:x} set class to {}", rc<uintptr_t>(this), m_class);
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

    Debug::log(LOG, "xwayland window {:x} -> association to {:x}", rc<uintptr_t>(m_xwaylandSurface.get()), rc<uintptr_t>(m_wlSurface->resource().get()));
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

    m_workspace = g_pCompositor->getMonitorFromVector(m_realPosition->goal() + m_realSize->goal() / 2.f)->m_activeWorkspace;

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
    for (auto const& w : Desktop::focusState()->windowHistory()) {
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

Vector2D CWindow::requestedMinSize() {
    bool hasSizeHints = m_xwaylandSurface ? m_xwaylandSurface->m_sizeHints : false;
    bool hasTopLevel  = m_xdgSurface ? m_xdgSurface->m_toplevel : false;
    if ((m_isX11 && !hasSizeHints) || (!m_isX11 && !hasTopLevel))
        return Vector2D(1, 1);

    Vector2D minSize = m_isX11 ? Vector2D(m_xwaylandSurface->m_sizeHints->min_width, m_xwaylandSurface->m_sizeHints->min_height) : m_xdgSurface->m_toplevel->layoutMinSize();

    minSize = minSize.clamp({1, 1});

    return minSize;
}

Vector2D CWindow::requestedMaxSize() {
    constexpr int NO_MAX_SIZE_LIMIT = 99999;
    if (((m_isX11 && !m_xwaylandSurface->m_sizeHints) || (!m_isX11 && (!m_xdgSurface || !m_xdgSurface->m_toplevel)) || m_ruleApplicator->noMaxSize().valueOrDefault()))
        return Vector2D(NO_MAX_SIZE_LIMIT, NO_MAX_SIZE_LIMIT);

    Vector2D maxSize = m_isX11 ? Vector2D(m_xwaylandSurface->m_sizeHints->max_width, m_xwaylandSurface->m_sizeHints->max_height) : m_xdgSurface->m_toplevel->layoutMaxSize();

    if (maxSize.x < 5)
        maxSize.x = NO_MAX_SIZE_LIMIT;
    if (maxSize.y < 5)
        maxSize.y = NO_MAX_SIZE_LIMIT;

    return maxSize;
}

Vector2D CWindow::realToReportSize() {
    if (!m_isX11)
        return m_realSize->goal().clamp(Vector2D{0, 0}, Vector2D{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()});

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  REPORTSIZE = m_realSize->goal().clamp(Vector2D{1, 1}, Vector2D{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()});
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
    const auto  SIZE     = size.clamp(Vector2D{1, 1}, Vector2D{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()});
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

    Debug::log(TRACE, "sendWindowSize: window:{:x},title:{} with real pos {}, real size {} (force: {})", rc<uintptr_t>(this), this->m_title, m_realPosition->goal(),
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

    Debug::log(INFO, "ContentType for window {}", sc<int>(contentType));
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
