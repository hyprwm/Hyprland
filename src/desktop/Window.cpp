#include <hyprutils/animation/AnimatedVariable.hpp>
#include <re2/re2.h>

#include <any>
#include <bit>
#include <string_view>
#include <algorithm>
#include "Window.hpp"
#include "../Compositor.hpp"
#include "../render/decorations/CHyprDropShadowDecoration.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../render/decorations/CHyprBorderDecoration.hpp"
#include "../config/ConfigValue.hpp"
#include "../managers/TokenManager.hpp"
#include "../managers/AnimationManager.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/ContentType.hpp"
#include "../xwayland/XWayland.hpp"
#include "../helpers/Color.hpp"
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

    pWindow->m_pSelf  = pWindow;
    pWindow->m_bIsX11 = true;

    g_pAnimationManager->createAnimation(Vector2D(0, 0), pWindow->m_vRealPosition, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(Vector2D(0, 0), pWindow->m_vRealSize, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fBorderFadeAnimationProgress, g_pConfigManager->getAnimationPropertyConfig("border"), pWindow, AVARDAMAGE_BORDER);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fBorderAngleAnimationProgress, g_pConfigManager->getAnimationPropertyConfig("borderangle"), pWindow, AVARDAMAGE_BORDER);
    g_pAnimationManager->createAnimation(1.f, pWindow->m_fAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(1.f, pWindow->m_fActiveInactiveAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeSwitch"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(CHyprColor(), pWindow->m_cRealShadowColor, g_pConfigManager->getAnimationPropertyConfig("fadeShadow"), pWindow, AVARDAMAGE_SHADOW);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fDimPercent, g_pConfigManager->getAnimationPropertyConfig("fadeDim"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fMovingToWorkspaceAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeOut"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fMovingFromWorkspaceAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);

    pWindow->addWindowDeco(makeUnique<CHyprDropShadowDecoration>(pWindow));
    pWindow->addWindowDeco(makeUnique<CHyprBorderDecoration>(pWindow));

    return pWindow;
}

PHLWINDOW CWindow::create(SP<CXDGSurfaceResource> resource) {
    PHLWINDOW pWindow = SP<CWindow>(new CWindow(resource));

    pWindow->m_pSelf           = pWindow;
    resource->toplevel->window = pWindow;

    g_pAnimationManager->createAnimation(Vector2D(0, 0), pWindow->m_vRealPosition, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(Vector2D(0, 0), pWindow->m_vRealSize, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fBorderFadeAnimationProgress, g_pConfigManager->getAnimationPropertyConfig("border"), pWindow, AVARDAMAGE_BORDER);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fBorderAngleAnimationProgress, g_pConfigManager->getAnimationPropertyConfig("borderangle"), pWindow, AVARDAMAGE_BORDER);
    g_pAnimationManager->createAnimation(1.f, pWindow->m_fAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(1.f, pWindow->m_fActiveInactiveAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeSwitch"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(CHyprColor(), pWindow->m_cRealShadowColor, g_pConfigManager->getAnimationPropertyConfig("fadeShadow"), pWindow, AVARDAMAGE_SHADOW);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fDimPercent, g_pConfigManager->getAnimationPropertyConfig("fadeDim"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fMovingToWorkspaceAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeOut"), pWindow, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.f, pWindow->m_fMovingFromWorkspaceAlpha, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);

    pWindow->addWindowDeco(makeUnique<CHyprDropShadowDecoration>(pWindow));
    pWindow->addWindowDeco(makeUnique<CHyprBorderDecoration>(pWindow));

    pWindow->m_pWLSurface->assign(pWindow->m_pXDGSurface->surface.lock(), pWindow);

    return pWindow;
}

CWindow::CWindow(SP<CXDGSurfaceResource> resource) : m_pXDGSurface(resource) {
    m_pWLSurface = CWLSurface::create();

    listeners.map            = m_pXDGSurface->events.map.registerListener([this](std::any d) { Events::listener_mapWindow(this, nullptr); });
    listeners.ack            = m_pXDGSurface->events.ack.registerListener([this](std::any d) { onAck(std::any_cast<uint32_t>(d)); });
    listeners.unmap          = m_pXDGSurface->events.unmap.registerListener([this](std::any d) { Events::listener_unmapWindow(this, nullptr); });
    listeners.destroy        = m_pXDGSurface->events.destroy.registerListener([this](std::any d) { Events::listener_destroyWindow(this, nullptr); });
    listeners.commit         = m_pXDGSurface->events.commit.registerListener([this](std::any d) { Events::listener_commitWindow(this, nullptr); });
    listeners.updateState    = m_pXDGSurface->toplevel->events.stateChanged.registerListener([this](std::any d) { onUpdateState(); });
    listeners.updateMetadata = m_pXDGSurface->toplevel->events.metadataChanged.registerListener([this](std::any d) { onUpdateMeta(); });
}

CWindow::CWindow(SP<CXWaylandSurface> surface) : m_pXWaylandSurface(surface) {
    m_pWLSurface = CWLSurface::create();

    listeners.map            = m_pXWaylandSurface->events.map.registerListener([this](std::any d) { Events::listener_mapWindow(this, nullptr); });
    listeners.unmap          = m_pXWaylandSurface->events.unmap.registerListener([this](std::any d) { Events::listener_unmapWindow(this, nullptr); });
    listeners.destroy        = m_pXWaylandSurface->events.destroy.registerListener([this](std::any d) { Events::listener_destroyWindow(this, nullptr); });
    listeners.commit         = m_pXWaylandSurface->events.commit.registerListener([this](std::any d) { Events::listener_commitWindow(this, nullptr); });
    listeners.configure      = m_pXWaylandSurface->events.configure.registerListener([this](std::any d) { onX11Configure(std::any_cast<CBox>(d)); });
    listeners.updateState    = m_pXWaylandSurface->events.stateChanged.registerListener([this](std::any d) { onUpdateState(); });
    listeners.updateMetadata = m_pXWaylandSurface->events.metadataChanged.registerListener([this](std::any d) { onUpdateMeta(); });
    listeners.resourceChange = m_pXWaylandSurface->events.resourceChange.registerListener([this](std::any d) { onResourceChangeX11(); });
    listeners.activate       = m_pXWaylandSurface->events.activate.registerListener([this](std::any d) { Events::listener_activateX11(this, nullptr); });

    if (m_pXWaylandSurface->overrideRedirect)
        listeners.setGeometry = m_pXWaylandSurface->events.setGeometry.registerListener([this](std::any d) { Events::listener_unmanagedSetGeometry(this, nullptr); });
}

CWindow::~CWindow() {
    if (g_pCompositor->m_pLastWindow.lock().get() == this) {
        g_pCompositor->m_pLastFocus.reset();
        g_pCompositor->m_pLastWindow.reset();
    }

    events.destroy.emit();

    if (!g_pHyprOpenGL)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_mWindowFramebuffers, [&](const auto& other) { return !other.first.lock() || other.first.lock().get() == this; });
}

SBoxExtents CWindow::getFullWindowExtents() {
    if (m_bFadingOut)
        return m_eOriginalClosedExtents;

    const int BORDERSIZE = getRealBorderSize();

    if (m_sWindowData.dimAround.valueOrDefault()) {
        if (const auto PMONITOR = m_pMonitor.lock(); PMONITOR)
            return {{m_vRealPosition->value().x - PMONITOR->vecPosition.x, m_vRealPosition->value().y - PMONITOR->vecPosition.y},
                    {PMONITOR->vecSize.x - (m_vRealPosition->value().x - PMONITOR->vecPosition.x), PMONITOR->vecSize.y - (m_vRealPosition->value().y - PMONITOR->vecPosition.y)}};
    }

    SBoxExtents maxExtents = {{BORDERSIZE + 2, BORDERSIZE + 2}, {BORDERSIZE + 2, BORDERSIZE + 2}};

    const auto  EXTENTS = g_pDecorationPositioner->getWindowDecorationExtents(m_pSelf.lock());

    if (EXTENTS.topLeft.x > maxExtents.topLeft.x)
        maxExtents.topLeft.x = EXTENTS.topLeft.x;

    if (EXTENTS.topLeft.y > maxExtents.topLeft.y)
        maxExtents.topLeft.y = EXTENTS.topLeft.y;

    if (EXTENTS.bottomRight.x > maxExtents.bottomRight.x)
        maxExtents.bottomRight.x = EXTENTS.bottomRight.x;

    if (EXTENTS.bottomRight.y > maxExtents.bottomRight.y)
        maxExtents.bottomRight.y = EXTENTS.bottomRight.y;

    if (m_pWLSurface->exists() && !m_bIsX11 && m_pPopupHead) {
        CBox surfaceExtents = {0, 0, 0, 0};
        // TODO: this could be better, perhaps make a getFullWindowRegion?
        m_pPopupHead->breadthfirst(
            [](WP<CPopup> popup, void* data) {
                if (!popup->m_pWLSurface || !popup->m_pWLSurface->resource())
                    return;

                CBox* pSurfaceExtents = (CBox*)data;
                CBox  surf            = CBox{popup->coordsRelativeToParent(), popup->size()};
                if (surf.x < pSurfaceExtents->x)
                    pSurfaceExtents->x = surf.x;
                if (surf.y < pSurfaceExtents->y)
                    pSurfaceExtents->y = surf.y;
                if (surf.x + surf.w > pSurfaceExtents->width)
                    pSurfaceExtents->width = surf.x + surf.w - pSurfaceExtents->x;
                if (surf.y + surf.h > pSurfaceExtents->height)
                    pSurfaceExtents->height = surf.y + surf.h - pSurfaceExtents->y;
            },
            &surfaceExtents);

        if (-surfaceExtents.x > maxExtents.topLeft.x)
            maxExtents.topLeft.x = -surfaceExtents.x;

        if (-surfaceExtents.y > maxExtents.topLeft.y)
            maxExtents.topLeft.y = -surfaceExtents.y;

        if (surfaceExtents.x + surfaceExtents.width > m_pWLSurface->resource()->current.size.x + maxExtents.bottomRight.x)
            maxExtents.bottomRight.x = surfaceExtents.x + surfaceExtents.width - m_pWLSurface->resource()->current.size.x;

        if (surfaceExtents.y + surfaceExtents.height > m_pWLSurface->resource()->current.size.y + maxExtents.bottomRight.y)
            maxExtents.bottomRight.y = surfaceExtents.y + surfaceExtents.height - m_pWLSurface->resource()->current.size.y;
    }

    return maxExtents;
}

CBox CWindow::getFullWindowBoundingBox() {
    if (m_sWindowData.dimAround.valueOrDefault()) {
        if (const auto PMONITOR = m_pMonitor.lock(); PMONITOR)
            return {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};
    }

    auto maxExtents = getFullWindowExtents();

    CBox finalBox = {m_vRealPosition->value().x - maxExtents.topLeft.x, m_vRealPosition->value().y - maxExtents.topLeft.y,
                     m_vRealSize->value().x + maxExtents.topLeft.x + maxExtents.bottomRight.x, m_vRealSize->value().y + maxExtents.topLeft.y + maxExtents.bottomRight.y};

    return finalBox;
}

CBox CWindow::getWindowIdealBoundingBoxIgnoreReserved() {
    const auto PMONITOR = m_pMonitor.lock();

    if (!PMONITOR)
        return {m_vPosition, m_vSize};

    auto POS  = m_vPosition;
    auto SIZE = m_vSize;

    if (isFullscreen()) {
        POS  = PMONITOR->vecPosition;
        SIZE = PMONITOR->vecSize;

        return CBox{(int)POS.x, (int)POS.y, (int)SIZE.x, (int)SIZE.y};
    }

    if (DELTALESSTHAN(POS.y - PMONITOR->vecPosition.y, PMONITOR->vecReservedTopLeft.y, 1)) {
        POS.y = PMONITOR->vecPosition.y;
        SIZE.y += PMONITOR->vecReservedTopLeft.y;
    }
    if (DELTALESSTHAN(POS.x - PMONITOR->vecPosition.x, PMONITOR->vecReservedTopLeft.x, 1)) {
        POS.x = PMONITOR->vecPosition.x;
        SIZE.x += PMONITOR->vecReservedTopLeft.x;
    }
    if (DELTALESSTHAN(POS.x + SIZE.x - PMONITOR->vecPosition.x, PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x, 1)) {
        SIZE.x += PMONITOR->vecReservedBottomRight.x;
    }
    if (DELTALESSTHAN(POS.y + SIZE.y - PMONITOR->vecPosition.y, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y, 1)) {
        SIZE.y += PMONITOR->vecReservedBottomRight.y;
    }

    return CBox{(int)POS.x, (int)POS.y, (int)SIZE.x, (int)SIZE.y};
}

CBox CWindow::getWindowBoxUnified(uint64_t properties) {
    if (m_sWindowData.dimAround.valueOrDefault()) {
        const auto PMONITOR = m_pMonitor.lock();
        if (PMONITOR)
            return {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};
    }

    SBoxExtents EXTENTS = {{0, 0}, {0, 0}};
    if (properties & RESERVED_EXTENTS)
        EXTENTS.addExtents(g_pDecorationPositioner->getWindowDecorationReserved(m_pSelf.lock()));
    if (properties & INPUT_EXTENTS)
        EXTENTS.addExtents(g_pDecorationPositioner->getWindowDecorationExtents(m_pSelf.lock(), true));
    if (properties & FULL_EXTENTS)
        EXTENTS.addExtents(g_pDecorationPositioner->getWindowDecorationExtents(m_pSelf.lock(), false));

    CBox box = {m_vRealPosition->value().x, m_vRealPosition->value().y, m_vRealSize->value().x, m_vRealSize->value().y};
    box.addExtents(EXTENTS);

    return box;
}

SBoxExtents CWindow::getFullWindowReservedArea() {
    return g_pDecorationPositioner->getWindowDecorationReserved(m_pSelf.lock());
}

void CWindow::updateWindowDecos() {

    if (!m_bIsMapped || isHidden())
        return;

    for (auto const& wd : m_vDecosToRemove) {
        for (auto it = m_dWindowDecorations.begin(); it != m_dWindowDecorations.end(); it++) {
            if (it->get() == wd) {
                g_pDecorationPositioner->uncacheDecoration(it->get());
                it = m_dWindowDecorations.erase(it);
                if (it == m_dWindowDecorations.end())
                    break;
            }
        }
    }

    g_pDecorationPositioner->onWindowUpdate(m_pSelf.lock());

    m_vDecosToRemove.clear();

    // make a copy because updateWindow can remove decos.
    std::vector<IHyprWindowDecoration*> decos;
    // reserve to avoid reallocations
    decos.reserve(m_dWindowDecorations.size());

    for (auto const& wd : m_dWindowDecorations) {
        decos.push_back(wd.get());
    }

    for (auto const& wd : decos) {
        if (std::find_if(m_dWindowDecorations.begin(), m_dWindowDecorations.end(), [wd](const auto& other) { return other.get() == wd; }) == m_dWindowDecorations.end())
            continue;
        wd->updateWindow(m_pSelf.lock());
    }
}

void CWindow::addWindowDeco(UP<IHyprWindowDecoration> deco) {
    m_dWindowDecorations.emplace_back(std::move(deco));
    g_pDecorationPositioner->forceRecalcFor(m_pSelf.lock());
    updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(m_pSelf.lock());
}

void CWindow::removeWindowDeco(IHyprWindowDecoration* deco) {
    m_vDecosToRemove.push_back(deco);
    g_pDecorationPositioner->forceRecalcFor(m_pSelf.lock());
    updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(m_pSelf.lock());
}

void CWindow::uncacheWindowDecos() {
    for (auto const& wd : m_dWindowDecorations) {
        g_pDecorationPositioner->uncacheDecoration(wd.get());
    }
}

bool CWindow::checkInputOnDecos(const eInputType type, const Vector2D& mouseCoords, std::any data) {
    if (type != INPUT_TYPE_DRAG_END && hasPopupAt(mouseCoords))
        return false;

    for (auto const& wd : m_dWindowDecorations) {
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
    if (!m_bIsX11) {
        if (!m_pXDGSurface || !m_pXDGSurface->owner /* happens at unmap */)
            return -1;

        wl_client_get_credentials(m_pXDGSurface->owner->client(), &PID, nullptr, nullptr);
    } else {
        if (!m_pXWaylandSurface)
            return -1;

        PID = m_pXWaylandSurface->pid;
    }

    return PID;
}

IHyprWindowDecoration* CWindow::getDecorationByType(eDecorationType type) {
    for (auto const& wd : m_dWindowDecorations) {
        if (wd->getDecorationType() == type)
            return wd.get();
    }

    return nullptr;
}

void CWindow::updateToplevel() {
    updateSurfaceScaleTransformDetails();
}

void CWindow::updateSurfaceScaleTransformDetails(bool force) {
    if (!m_bIsMapped || m_bHidden || g_pCompositor->m_bUnsafeState)
        return;

    const auto PLASTMONITOR = g_pCompositor->getMonitorFromID(m_iLastSurfaceMonitorID);

    m_iLastSurfaceMonitorID = monitorID();

    const auto PNEWMONITOR = m_pMonitor.lock();

    if (!PNEWMONITOR)
        return;

    if (PNEWMONITOR != PLASTMONITOR || force) {
        if (PLASTMONITOR && PLASTMONITOR->m_bEnabled && PNEWMONITOR != PLASTMONITOR)
            m_pWLSurface->resource()->breadthfirst([PLASTMONITOR](SP<CWLSurfaceResource> s, const Vector2D& offset, void* d) { s->leave(PLASTMONITOR->self.lock()); }, nullptr);

        m_pWLSurface->resource()->breadthfirst([PNEWMONITOR](SP<CWLSurfaceResource> s, const Vector2D& offset, void* d) { s->enter(PNEWMONITOR->self.lock()); }, nullptr);
    }

    const auto PMONITOR = m_pMonitor.lock();

    m_pWLSurface->resource()->breadthfirst(
        [PMONITOR](SP<CWLSurfaceResource> s, const Vector2D& offset, void* d) {
            const auto PSURFACE = CWLSurface::fromResource(s);
            if (PSURFACE && PSURFACE->m_fLastScale == PMONITOR->scale)
                return;

            g_pCompositor->setPreferredScaleForSurface(s, PMONITOR->scale);
            g_pCompositor->setPreferredTransformForSurface(s, PMONITOR->transform);
        },
        nullptr);
}

void CWindow::moveToWorkspace(PHLWORKSPACE pWorkspace) {
    if (m_pWorkspace == pWorkspace)
        return;

    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    if (!m_szInitialWorkspaceToken.empty()) {
        const auto TOKEN = g_pTokenManager->getToken(m_szInitialWorkspaceToken);
        if (TOKEN) {
            if (*PINITIALWSTRACKING == 2) {
                // persistent
                SInitialWorkspaceToken token = std::any_cast<SInitialWorkspaceToken>(TOKEN->data);
                if (token.primaryOwner.lock().get() == this) {
                    token.workspace = pWorkspace->getConfigName();
                    TOKEN->data     = token;
                }
            }
        }
    }

    static auto PCLOSEONLASTSPECIAL = CConfigValue<Hyprlang::INT>("misc:close_special_on_empty");

    const auto  OLDWORKSPACE = m_pWorkspace;

    if (OLDWORKSPACE->isVisible()) {
        m_fMovingToWorkspaceAlpha->setValueAndWarp(1.F);
        *m_fMovingToWorkspaceAlpha = 0.F;
        m_fMovingToWorkspaceAlpha->setCallbackOnEnd([this](auto) { m_iMonitorMovedFrom = -1; });
        m_iMonitorMovedFrom = OLDWORKSPACE ? OLDWORKSPACE->monitorID() : -1;
    }

    m_pWorkspace = pWorkspace;

    setAnimationsToMove();

    OLDWORKSPACE->updateWindows();
    OLDWORKSPACE->updateWindowData();
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(OLDWORKSPACE->monitorID());

    pWorkspace->updateWindows();
    pWorkspace->updateWindowData();
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (valid(pWorkspace)) {
        g_pEventManager->postEvent(SHyprIPCEvent{"movewindow", std::format("{:x},{}", (uintptr_t)this, pWorkspace->m_szName)});
        g_pEventManager->postEvent(SHyprIPCEvent{"movewindowv2", std::format("{:x},{},{}", (uintptr_t)this, pWorkspace->m_iID, pWorkspace->m_szName)});
        EMIT_HOOK_EVENT("moveWindow", (std::vector<std::any>{m_pSelf.lock(), pWorkspace}));
    }

    if (const auto SWALLOWED = m_pSwallowed.lock()) {
        if (SWALLOWED->m_bCurrentlySwallowed) {
            SWALLOWED->moveToWorkspace(pWorkspace);
            SWALLOWED->m_pMonitor = m_pMonitor;
        }
    }

    // update xwayland coords
    sendWindowSize(m_vRealSize->goal());

    if (OLDWORKSPACE && g_pCompositor->isWorkspaceSpecial(OLDWORKSPACE->m_iID) && OLDWORKSPACE->getWindows() == 0 && *PCLOSEONLASTSPECIAL) {
        if (const auto PMONITOR = OLDWORKSPACE->m_pMonitor.lock(); PMONITOR)
            PMONITOR->setSpecialWorkspace(nullptr);
    }
}

PHLWINDOW CWindow::x11TransientFor() {
    if (!m_pXWaylandSurface || !m_pXWaylandSurface->parent)
        return nullptr;

    auto                              s = m_pXWaylandSurface->parent;
    std::vector<SP<CXWaylandSurface>> visited;
    while (s) {
        // break loops. Some X apps make them, and it seems like it's valid behavior?!?!?!
        // TODO: we should reject loops being created in the first place.
        if (std::find(visited.begin(), visited.end(), s) != visited.end())
            break;

        visited.emplace_back(s.lock());
        s = s->parent;
    }

    if (s == m_pXWaylandSurface)
        return nullptr; // dead-ass circle

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pXWaylandSurface != s)
            continue;
        return w;
    }

    return nullptr;
}

void CWindow::onUnmap() {
    static auto PCLOSEONLASTSPECIAL = CConfigValue<Hyprlang::INT>("misc:close_special_on_empty");
    static auto PINITIALWSTRACKING  = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    if (!m_szInitialWorkspaceToken.empty()) {
        const auto TOKEN = g_pTokenManager->getToken(m_szInitialWorkspaceToken);
        if (TOKEN) {
            if (*PINITIALWSTRACKING == 2) {
                // persistent token, but the first window got removed so the token is gone
                SInitialWorkspaceToken token = std::any_cast<SInitialWorkspaceToken>(TOKEN->data);
                if (token.primaryOwner.lock().get() == this)
                    g_pTokenManager->removeToken(TOKEN);
            }
        }
    }

    m_iLastWorkspace = m_pWorkspace->m_iID;

    std::erase_if(g_pCompositor->m_vWindowFocusHistory, [&](const auto& other) { return other.expired() || other.lock().get() == this; });

    if (*PCLOSEONLASTSPECIAL && m_pWorkspace && m_pWorkspace->getWindows() == 0 && onSpecialWorkspace()) {
        const auto PMONITOR = m_pMonitor.lock();
        if (PMONITOR && PMONITOR->activeSpecialWorkspace && PMONITOR->activeSpecialWorkspace == m_pWorkspace)
            PMONITOR->setSpecialWorkspace(nullptr);
    }

    const auto PMONITOR = m_pMonitor.lock();

    if (PMONITOR && PMONITOR->solitaryClient.lock().get() == this)
        PMONITOR->solitaryClient.reset();

    if (m_pWorkspace) {
        m_pWorkspace->updateWindows();
        m_pWorkspace->updateWindowData();
    }
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    m_pWorkspace.reset();

    if (m_bIsX11)
        return;

    m_pSubsurfaceHead.reset();
    m_pPopupHead.reset();
}

void CWindow::onMap() {
    // JIC, reset the callbacks. If any are set, we'll make sure they are cleared so we don't accidentally unset them. (In case a window got remapped)
    m_vRealPosition->resetAllCallbacks();
    m_vRealSize->resetAllCallbacks();
    m_fBorderFadeAnimationProgress->resetAllCallbacks();
    m_fBorderAngleAnimationProgress->resetAllCallbacks();
    m_fActiveInactiveAlpha->resetAllCallbacks();
    m_fAlpha->resetAllCallbacks();
    m_cRealShadowColor->resetAllCallbacks();
    m_fDimPercent->resetAllCallbacks();
    m_fMovingToWorkspaceAlpha->resetAllCallbacks();
    m_fMovingFromWorkspaceAlpha->resetAllCallbacks();

    m_fMovingFromWorkspaceAlpha->setValueAndWarp(1.F);

    if (m_fBorderAngleAnimationProgress->enabled()) {
        m_fBorderAngleAnimationProgress->setValueAndWarp(0.f);
        m_fBorderAngleAnimationProgress->setCallbackOnEnd([&](WP<CBaseAnimatedVariable> p) { onBorderAngleAnimEnd(p); }, false);
        *m_fBorderAngleAnimationProgress = 1.f;
    }

    m_fMovingFromWorkspaceAlpha->setValueAndWarp(1.F);

    g_pCompositor->m_vWindowFocusHistory.push_back(m_pSelf);

    m_vReportedSize = m_vPendingReportedSize;
    m_bAnimatingIn  = true;

    updateSurfaceScaleTransformDetails(true);

    if (m_bIsX11)
        return;

    m_pSubsurfaceHead = CSubsurface::create(m_pSelf.lock());
    m_pPopupHead      = CPopup::create(m_pSelf.lock());
}

void CWindow::onBorderAngleAnimEnd(WP<CBaseAnimatedVariable> pav) {
    const auto PAV = pav.lock();
    if (!PAV)
        return;

    if (PAV->getStyle() != "loop" || !PAV->enabled())
        return;

    const auto PANIMVAR = dynamic_cast<CAnimatedVariable<float>*>(PAV.get());

    PANIMVAR->setCallbackOnEnd(nullptr); // we remove the callback here because otherwise setvalueandwarp will recurse this

    PANIMVAR->setValueAndWarp(0);
    *PANIMVAR = 1.f;

    PANIMVAR->setCallbackOnEnd([&](WP<CBaseAnimatedVariable> pav) { onBorderAngleAnimEnd(pav); }, false);
}

void CWindow::setHidden(bool hidden) {
    m_bHidden = hidden;

    if (hidden && g_pCompositor->m_pLastWindow.lock().get() == this) {
        g_pCompositor->m_pLastWindow.reset();
    }

    setSuspended(hidden);
}

bool CWindow::isHidden() {
    return m_bHidden;
}

void CWindow::applyDynamicRule(const SP<CWindowRule>& r) {
    const eOverridePriority priority = r->execRule ? PRIORITY_SET_PROP : PRIORITY_WINDOW_RULE;

    switch (r->ruleType) {
        case CWindowRule::RULE_TAG: {
            CVarList vars{r->szRule, 0, 's', true};

            if (vars.size() == 2 && vars[0] == "tag")
                m_tags.applyTag(vars[1], true);
            else
                Debug::log(ERR, "Tag rule invalid: {}", r->szRule);
            break;
        }
        case CWindowRule::RULE_OPACITY: {
            try {
                CVarList vars(r->szRule, 0, ' ');

                int      opacityIDX = 0;

                for (auto const& r : vars) {
                    if (r == "opacity")
                        continue;

                    if (r == "override") {
                        if (opacityIDX == 1)
                            m_sWindowData.alpha = CWindowOverridableVar(SAlphaValue{m_sWindowData.alpha.value().m_fAlpha, true}, priority);
                        else if (opacityIDX == 2)
                            m_sWindowData.alphaInactive = CWindowOverridableVar(SAlphaValue{m_sWindowData.alphaInactive.value().m_fAlpha, true}, priority);
                        else if (opacityIDX == 3)
                            m_sWindowData.alphaFullscreen = CWindowOverridableVar(SAlphaValue{m_sWindowData.alphaFullscreen.value().m_fAlpha, true}, priority);
                    } else {
                        if (opacityIDX == 0) {
                            m_sWindowData.alpha = CWindowOverridableVar(SAlphaValue{std::stof(r), false}, priority);
                        } else if (opacityIDX == 1) {
                            m_sWindowData.alphaInactive = CWindowOverridableVar(SAlphaValue{std::stof(r), false}, priority);
                        } else if (opacityIDX == 2) {
                            m_sWindowData.alphaFullscreen = CWindowOverridableVar(SAlphaValue{std::stof(r), false}, priority);
                        } else {
                            throw std::runtime_error("more than 3 alpha values");
                        }

                        opacityIDX++;
                    }
                }

                if (opacityIDX == 1) {
                    m_sWindowData.alphaInactive   = m_sWindowData.alpha;
                    m_sWindowData.alphaFullscreen = m_sWindowData.alpha;
                }
            } catch (std::exception& e) { Debug::log(ERR, "Opacity rule \"{}\" failed with: {}", r->szRule, e.what()); }
            break;
        }
        case CWindowRule::RULE_ANIMATION: {
            auto STYLE                   = r->szRule.substr(r->szRule.find_first_of(' ') + 1);
            m_sWindowData.animationStyle = CWindowOverridableVar(STYLE, priority);
            break;
        }
        case CWindowRule::RULE_BORDERCOLOR: {
            try {
                // Each vector will only get used if it has at least one color
                CGradientValueData activeBorderGradient   = {};
                CGradientValueData inactiveBorderGradient = {};
                bool               active                 = true;
                CVarList           colorsAndAngles        = CVarList(trim(r->szRule.substr(r->szRule.find_first_of(' ') + 1)), 0, 's', true);

                // Basic form has only two colors, everything else can be parsed as a gradient
                if (colorsAndAngles.size() == 2 && !colorsAndAngles[1].contains("deg")) {
                    m_sWindowData.activeBorderColor   = CWindowOverridableVar(CGradientValueData(CHyprColor(configStringToInt(colorsAndAngles[0]).value_or(0))), priority);
                    m_sWindowData.inactiveBorderColor = CWindowOverridableVar(CGradientValueData(CHyprColor(configStringToInt(colorsAndAngles[1]).value_or(0))), priority);
                    return;
                }

                for (auto const& token : colorsAndAngles) {
                    // The first angle, or an explicit "0deg", splits the two gradients
                    if (active && token.contains("deg")) {
                        activeBorderGradient.m_fAngle = std::stoi(token.substr(0, token.size() - 3)) * (PI / 180.0);
                        active                        = false;
                    } else if (token.contains("deg"))
                        inactiveBorderGradient.m_fAngle = std::stoi(token.substr(0, token.size() - 3)) * (PI / 180.0);
                    else if (active)
                        activeBorderGradient.m_vColors.push_back(configStringToInt(token).value_or(0));
                    else
                        inactiveBorderGradient.m_vColors.push_back(configStringToInt(token).value_or(0));
                }

                activeBorderGradient.updateColorsOk();

                // Includes sanity checks for the number of colors in each gradient
                if (activeBorderGradient.m_vColors.size() > 10 || inactiveBorderGradient.m_vColors.size() > 10)
                    Debug::log(WARN, "Bordercolor rule \"{}\" has more than 10 colors in one gradient, ignoring", r->szRule);
                else if (activeBorderGradient.m_vColors.empty())
                    Debug::log(WARN, "Bordercolor rule \"{}\" has no colors, ignoring", r->szRule);
                else if (inactiveBorderGradient.m_vColors.empty())
                    m_sWindowData.activeBorderColor = CWindowOverridableVar(activeBorderGradient, priority);
                else {
                    m_sWindowData.activeBorderColor   = CWindowOverridableVar(activeBorderGradient, priority);
                    m_sWindowData.inactiveBorderColor = CWindowOverridableVar(inactiveBorderGradient, priority);
                }
            } catch (std::exception& e) { Debug::log(ERR, "BorderColor rule \"{}\" failed with: {}", r->szRule, e.what()); }
            break;
        }
        case CWindowRule::RULE_IDLEINHIBIT: {
            auto IDLERULE = r->szRule.substr(r->szRule.find_first_of(' ') + 1);

            if (IDLERULE == "none")
                m_eIdleInhibitMode = IDLEINHIBIT_NONE;
            else if (IDLERULE == "always")
                m_eIdleInhibitMode = IDLEINHIBIT_ALWAYS;
            else if (IDLERULE == "focus")
                m_eIdleInhibitMode = IDLEINHIBIT_FOCUS;
            else if (IDLERULE == "fullscreen")
                m_eIdleInhibitMode = IDLEINHIBIT_FULLSCREEN;
            else
                Debug::log(ERR, "Rule idleinhibit: unknown mode {}", IDLERULE);
            break;
        }
        case CWindowRule::RULE_MAXSIZE: {
            try {
                if (!m_bIsFloating)
                    return;
                const auto VEC = configStringToVector2D(r->szRule.substr(8));
                if (VEC.x < 1 || VEC.y < 1) {
                    Debug::log(ERR, "Invalid size for maxsize");
                    return;
                }

                m_sWindowData.maxSize = CWindowOverridableVar(VEC, priority);
                clampWindowSize(std::nullopt, m_sWindowData.maxSize.value());

            } catch (std::exception& e) { Debug::log(ERR, "maxsize rule \"{}\" failed with: {}", r->szRule, e.what()); }
            break;
        }
        case CWindowRule::RULE_MINSIZE: {
            try {
                if (!m_bIsFloating)
                    return;
                const auto VEC = configStringToVector2D(r->szRule.substr(8));
                if (VEC.x < 1 || VEC.y < 1) {
                    Debug::log(ERR, "Invalid size for minsize");
                    return;
                }

                m_sWindowData.minSize = CWindowOverridableVar(VEC, priority);
                clampWindowSize(m_sWindowData.minSize.value(), std::nullopt);

                if (m_sGroupData.pNextWindow.expired())
                    setHidden(false);
            } catch (std::exception& e) { Debug::log(ERR, "minsize rule \"{}\" failed with: {}", r->szRule, e.what()); }
            break;
        }
        case CWindowRule::RULE_RENDERUNFOCUSED: {
            m_sWindowData.renderUnfocused = CWindowOverridableVar(true, priority);
            g_pHyprRenderer->addWindowToRenderUnfocused(m_pSelf.lock());
            break;
        }
        case CWindowRule::RULE_PROP: {
            const CVarList VARS(r->szRule, 0, ' ');
            if (auto search = g_pConfigManager->miWindowProperties.find(VARS[1]); search != g_pConfigManager->miWindowProperties.end()) {
                try {
                    *(search->second(m_pSelf.lock())) = CWindowOverridableVar(std::stoi(VARS[2]), priority);
                } catch (std::exception& e) { Debug::log(ERR, "Rule \"{}\" failed with: {}", r->szRule, e.what()); }
            } else if (auto search = g_pConfigManager->mfWindowProperties.find(VARS[1]); search != g_pConfigManager->mfWindowProperties.end()) {
                try {
                    *(search->second(m_pSelf.lock())) = CWindowOverridableVar(std::stof(VARS[2]), priority);
                } catch (std::exception& e) { Debug::log(ERR, "Rule \"{}\" failed with: {}", r->szRule, e.what()); }
            } else if (auto search = g_pConfigManager->mbWindowProperties.find(VARS[1]); search != g_pConfigManager->mbWindowProperties.end()) {
                try {
                    *(search->second(m_pSelf.lock())) = CWindowOverridableVar(VARS[2].empty() ? true : (bool)std::stoi(VARS[2]), priority);
                } catch (std::exception& e) { Debug::log(ERR, "Rule \"{}\" failed with: {}", r->szRule, e.what()); }
            }
            break;
        }
        default: break;
    }
}

void CWindow::updateDynamicRules() {
    m_sWindowData.alpha.unset(PRIORITY_WINDOW_RULE);
    m_sWindowData.alphaInactive.unset(PRIORITY_WINDOW_RULE);
    m_sWindowData.alphaFullscreen.unset(PRIORITY_WINDOW_RULE);

    unsetWindowData(PRIORITY_WINDOW_RULE);

    m_sWindowData.animationStyle.unset(PRIORITY_WINDOW_RULE);
    m_sWindowData.maxSize.unset(PRIORITY_WINDOW_RULE);
    m_sWindowData.minSize.unset(PRIORITY_WINDOW_RULE);

    m_sWindowData.activeBorderColor.unset(PRIORITY_WINDOW_RULE);
    m_sWindowData.inactiveBorderColor.unset(PRIORITY_WINDOW_RULE);

    m_sWindowData.renderUnfocused.unset(PRIORITY_WINDOW_RULE);

    m_eIdleInhibitMode = IDLEINHIBIT_NONE;

    m_tags.removeDynamicTags();

    m_vMatchedRules = g_pConfigManager->getMatchingRules(m_pSelf.lock());
    for (const auto& r : m_vMatchedRules) {
        applyDynamicRule(r);
    }

    EMIT_HOOK_EVENT("windowUpdateRules", m_pSelf.lock());

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
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
    double x0 = m_vRealPosition->value().x + ROUNDING;
    double y0 = m_vRealPosition->value().y + ROUNDING;
    double x1 = m_vRealPosition->value().x + m_vRealSize->value().x - ROUNDING;
    double y1 = m_vRealPosition->value().y + m_vRealSize->value().y - ROUNDING;

    if (x < x0 && y < y0) {
        return std::pow(x0 - x, ROUNDINGPOWER) + std::pow(y0 - y, ROUNDINGPOWER) > std::pow((double)ROUNDING, ROUNDINGPOWER);
    }
    if (x > x1 && y < y0) {
        return std::pow(x - x1, ROUNDINGPOWER) + std::pow(y0 - y, ROUNDINGPOWER) > std::pow((double)ROUNDING, ROUNDINGPOWER);
    }
    if (x < x0 && y > y1) {
        return std::pow(x0 - x, ROUNDINGPOWER) + std::pow(y - y1, ROUNDINGPOWER) > std::pow((double)ROUNDING, ROUNDINGPOWER);
    }
    if (x > x1 && y > y1) {
        return std::pow(x - x1, ROUNDINGPOWER) + std::pow(y - y1, ROUNDINGPOWER) > std::pow((double)ROUNDING, ROUNDINGPOWER);
    }

    return false;
}

// checks if the wayland window has a popup at pos
bool CWindow::hasPopupAt(const Vector2D& pos) {
    if (m_bIsX11)
        return false;

    auto popup = m_pPopupHead->at(pos);

    return popup && popup->m_pWLSurface->resource();
}

void CWindow::applyGroupRules() {
    if ((m_eGroupRules & GROUP_SET && m_bFirstMap) || m_eGroupRules & GROUP_SET_ALWAYS)
        createGroup();

    if (m_sGroupData.pNextWindow.lock() && ((m_eGroupRules & GROUP_LOCK && m_bFirstMap) || m_eGroupRules & GROUP_LOCK_ALWAYS))
        getGroupHead()->m_sGroupData.locked = true;
}

void CWindow::createGroup() {
    if (m_sGroupData.deny) {
        Debug::log(LOG, "createGroup: window:{:x},title:{} is denied as a group, ignored", (uintptr_t)this, this->m_szTitle);
        return;
    }

    if (m_sGroupData.pNextWindow.expired()) {
        m_sGroupData.pNextWindow = m_pSelf;
        m_sGroupData.head        = true;
        m_sGroupData.locked      = false;
        m_sGroupData.deny        = false;

        addWindowDeco(makeUnique<CHyprGroupBarDecoration>(m_pSelf.lock()));

        if (m_pWorkspace) {
            m_pWorkspace->updateWindows();
            m_pWorkspace->updateWindowData();
        }
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();

        g_pEventManager->postEvent(SHyprIPCEvent{"togglegroup", std::format("1,{:x}", (uintptr_t)this)});
    }
}

void CWindow::destroyGroup() {
    if (m_sGroupData.pNextWindow.lock().get() == this) {
        if (m_eGroupRules & GROUP_SET_ALWAYS) {
            Debug::log(LOG, "destoryGroup: window:{:x},title:{} has rule [group set always], ignored", (uintptr_t)this, this->m_szTitle);
            return;
        }
        m_sGroupData.pNextWindow.reset();
        m_sGroupData.head = false;
        updateWindowDecos();
        if (m_pWorkspace) {
            m_pWorkspace->updateWindows();
            m_pWorkspace->updateWindowData();
        }
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();

        g_pEventManager->postEvent(SHyprIPCEvent{"togglegroup", std::format("0,{:x}", (uintptr_t)this)});
        return;
    }

    std::string            addresses;
    PHLWINDOW              curr = m_pSelf.lock();
    std::vector<PHLWINDOW> members;
    do {
        const auto PLASTWIN = curr;
        curr                = curr->m_sGroupData.pNextWindow.lock();
        PLASTWIN->m_sGroupData.pNextWindow.reset();
        curr->setHidden(false);
        members.push_back(curr);

        addresses += std::format("{:x},", (uintptr_t)curr.get());
    } while (curr.get() != this);

    for (auto const& w : members) {
        if (w->m_sGroupData.head)
            g_pLayoutManager->getCurrentLayout()->onWindowRemoved(curr);
        w->m_sGroupData.head = false;
    }

    const bool GROUPSLOCKEDPREV        = g_pKeybindManager->m_bGroupsLocked;
    g_pKeybindManager->m_bGroupsLocked = true;
    for (auto const& w : members) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(w);
        w->updateWindowDecos();
    }
    g_pKeybindManager->m_bGroupsLocked = GROUPSLOCKEDPREV;

    if (m_pWorkspace) {
        m_pWorkspace->updateWindows();
        m_pWorkspace->updateWindowData();
    }
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitorID());
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (!addresses.empty())
        addresses.pop_back();
    g_pEventManager->postEvent(SHyprIPCEvent{"togglegroup", std::format("0,{}", addresses)});
}

PHLWINDOW CWindow::getGroupHead() {
    PHLWINDOW curr = m_pSelf.lock();
    while (!curr->m_sGroupData.head)
        curr = curr->m_sGroupData.pNextWindow.lock();
    return curr;
}

PHLWINDOW CWindow::getGroupTail() {
    PHLWINDOW curr = m_pSelf.lock();
    while (!curr->m_sGroupData.pNextWindow->m_sGroupData.head)
        curr = curr->m_sGroupData.pNextWindow.lock();
    return curr;
}

PHLWINDOW CWindow::getGroupCurrent() {
    PHLWINDOW curr = m_pSelf.lock();
    while (curr->isHidden())
        curr = curr->m_sGroupData.pNextWindow.lock();
    return curr;
}

int CWindow::getGroupSize() {
    int       size = 1;
    PHLWINDOW curr = m_pSelf.lock();
    while (curr->m_sGroupData.pNextWindow.lock().get() != this) {
        curr = curr->m_sGroupData.pNextWindow.lock();
        size++;
    }
    return size;
}

bool CWindow::canBeGroupedInto(PHLWINDOW pWindow) {
    static auto ALLOWGROUPMERGE       = CConfigValue<Hyprlang::INT>("group:merge_groups_on_drag");
    bool        isGroup               = m_sGroupData.pNextWindow;
    bool        disallowDragIntoGroup = g_pInputManager->m_bWasDraggingWindow && isGroup && !bool(*ALLOWGROUPMERGE);
    return !g_pKeybindManager->m_bGroupsLocked                                                 // global group lock disengaged
        && ((m_eGroupRules & GROUP_INVADE && m_bFirstMap)                                      // window ignore local group locks, or
            || (!pWindow->getGroupHead()->m_sGroupData.locked                                  //      target unlocked
                && !(m_sGroupData.pNextWindow.lock() && getGroupHead()->m_sGroupData.locked))) //      source unlocked or isn't group
        && !m_sGroupData.deny                                                                  // source is not denied entry
        && !(m_eGroupRules & GROUP_BARRED && m_bFirstMap)                                      // group rule doesn't prevent adding window
        && !disallowDragIntoGroup;                                                             // config allows groups to be merged
}

PHLWINDOW CWindow::getGroupWindowByIndex(int index) {
    const int SIZE = getGroupSize();
    index          = ((index % SIZE) + SIZE) % SIZE;
    PHLWINDOW curr = getGroupHead();
    while (index > 0) {
        curr = curr->m_sGroupData.pNextWindow.lock();
        index--;
    }
    return curr;
}

void CWindow::setGroupCurrent(PHLWINDOW pWindow) {
    PHLWINDOW curr     = m_sGroupData.pNextWindow.lock();
    bool      isMember = false;
    while (curr.get() != this) {
        if (curr == pWindow) {
            isMember = true;
            break;
        }
        curr = curr->m_sGroupData.pNextWindow.lock();
    }

    if (!isMember && pWindow.get() != this)
        return;

    const auto PCURRENT   = getGroupCurrent();
    const bool FULLSCREEN = PCURRENT->isFullscreen();
    const auto WORKSPACE  = PCURRENT->m_pWorkspace;
    const auto MODE       = PCURRENT->m_sFullscreenState.internal;

    const auto CURRENTISFOCUS = PCURRENT == g_pCompositor->m_pLastWindow.lock();

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreenInternal(PCURRENT, FSMODE_NONE);

    const auto PWINDOWSIZE = PCURRENT->m_vRealSize->goal();
    const auto PWINDOWPOS  = PCURRENT->m_vRealPosition->goal();

    PCURRENT->setHidden(true);
    pWindow->setHidden(false); // can remove m_pLastWindow

    g_pLayoutManager->getCurrentLayout()->replaceWindowDataWith(PCURRENT, pWindow);

    if (PCURRENT->m_bIsFloating) {
        pWindow->m_vRealPosition->setValueAndWarp(PWINDOWPOS);
        pWindow->m_vRealSize->setValueAndWarp(PWINDOWSIZE);
    }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (CURRENTISFOCUS)
        g_pCompositor->focusWindow(pWindow);

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreenInternal(pWindow, MODE);

    g_pHyprRenderer->damageWindow(pWindow);

    pWindow->updateWindowDecos();
}

void CWindow::insertWindowToGroup(PHLWINDOW pWindow) {
    const auto BEGINAT = m_pSelf.lock();
    const auto ENDAT   = m_sGroupData.pNextWindow.lock();

    if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
        pWindow->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(pWindow));

    if (!pWindow->m_sGroupData.pNextWindow.lock()) {
        BEGINAT->m_sGroupData.pNextWindow = pWindow;
        pWindow->m_sGroupData.pNextWindow = ENDAT;
        pWindow->m_sGroupData.head        = false;
        return;
    }

    const auto SHEAD = pWindow->getGroupHead();
    const auto STAIL = pWindow->getGroupTail();

    SHEAD->m_sGroupData.head          = false;
    BEGINAT->m_sGroupData.pNextWindow = SHEAD;
    STAIL->m_sGroupData.pNextWindow   = ENDAT;
}

PHLWINDOW CWindow::getGroupPrevious() {
    PHLWINDOW curr = m_sGroupData.pNextWindow.lock();

    while (curr != m_pSelf.lock() && curr->m_sGroupData.pNextWindow.lock().get() != this)
        curr = curr->m_sGroupData.pNextWindow.lock();

    return curr;
}

void CWindow::switchWithWindowInGroup(PHLWINDOW pWindow) {
    if (!m_sGroupData.pNextWindow.lock() || !pWindow->m_sGroupData.pNextWindow.lock())
        return;

    if (m_sGroupData.pNextWindow.lock() == pWindow) { // A -> this -> pWindow -> B >> A -> pWindow -> this -> B
        getGroupPrevious()->m_sGroupData.pNextWindow = pWindow;
        m_sGroupData.pNextWindow                     = pWindow->m_sGroupData.pNextWindow;
        pWindow->m_sGroupData.pNextWindow            = m_pSelf;

    } else if (pWindow->m_sGroupData.pNextWindow.lock().get() == this) { // A -> pWindow -> this -> B >> A -> this -> pWindow -> B
        pWindow->getGroupPrevious()->m_sGroupData.pNextWindow = m_pSelf;
        pWindow->m_sGroupData.pNextWindow                     = m_sGroupData.pNextWindow;
        m_sGroupData.pNextWindow                              = pWindow;

    } else { // A -> this -> B | C -> pWindow -> D >> A -> pWindow -> B | C -> this -> D
        std::swap(m_sGroupData.pNextWindow, pWindow->m_sGroupData.pNextWindow);
        std::swap(getGroupPrevious()->m_sGroupData.pNextWindow, pWindow->getGroupPrevious()->m_sGroupData.pNextWindow);
    }

    std::swap(m_sGroupData.head, pWindow->m_sGroupData.head);
    std::swap(m_sGroupData.locked, pWindow->m_sGroupData.locked);
}

void CWindow::updateGroupOutputs() {
    if (m_sGroupData.pNextWindow.expired())
        return;

    PHLWINDOW  curr = m_sGroupData.pNextWindow.lock();

    const auto WS = m_pWorkspace;

    while (curr.get() != this) {
        curr->m_pMonitor = m_pMonitor;
        curr->moveToWorkspace(WS);

        *curr->m_vRealPosition = m_vRealPosition->goal();
        *curr->m_vRealSize     = m_vRealSize->goal();

        curr = curr->m_sGroupData.pNextWindow.lock();
    }
}

Vector2D CWindow::middle() {
    return m_vRealPosition->goal() + m_vRealSize->goal() / 2.f;
}

bool CWindow::opaque() {
    if (m_fAlpha->value() != 1.f || m_fActiveInactiveAlpha->value() != 1.f)
        return false;

    if (m_vRealSize->goal().floor() != m_vReportedSize)
        return false;

    const auto PWORKSPACE = m_pWorkspace;

    if (m_pWLSurface->small() && !m_pWLSurface->m_bFillIgnoreSmall)
        return false;

    if (PWORKSPACE->m_fAlpha->value() != 1.f)
        return false;

    if (m_bIsX11 && m_pXWaylandSurface && m_pXWaylandSurface->surface && m_pXWaylandSurface->surface->current.texture)
        return m_pXWaylandSurface->surface->current.texture->m_bOpaque;

    if (!m_pWLSurface->resource() || !m_pWLSurface->resource()->current.texture)
        return false;

    // TODO: this is wrong
    const auto EXTENTS = m_pXDGSurface->surface->current.opaque.getExtents();
    if (EXTENTS.w >= m_pXDGSurface->surface->current.bufferSize.x && EXTENTS.h >= m_pXDGSurface->surface->current.bufferSize.y)
        return true;

    return m_pWLSurface->resource()->current.texture->m_bOpaque;
}

float CWindow::rounding() {
    static auto PROUNDING      = CConfigValue<Hyprlang::INT>("decoration:rounding");
    static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

    float       roundingPower = m_sWindowData.roundingPower.valueOr(*PROUNDINGPOWER);
    float       rounding      = m_sWindowData.rounding.valueOr(*PROUNDING) * (roundingPower / 2.0); /* Make perceived roundness consistent. */

    return m_sWindowData.noRounding.valueOrDefault() ? 0 : rounding;
}

float CWindow::roundingPower() {
    static auto PROUNDINGPOWER = CConfigValue<Hyprlang::FLOAT>("decoration:rounding_power");

    return m_sWindowData.roundingPower.valueOr(*PROUNDINGPOWER);
}

void CWindow::updateWindowData() {
    const auto PWORKSPACE    = m_pWorkspace;
    const auto WORKSPACERULE = PWORKSPACE ? g_pConfigManager->getWorkspaceRuleFor(PWORKSPACE) : SWorkspaceRule{};
    updateWindowData(WORKSPACERULE);
}

void CWindow::updateWindowData(const SWorkspaceRule& workspaceRule) {
    static auto PNOBORDERONFLOATING = CConfigValue<Hyprlang::INT>("general:no_border_on_floating");

    if (*PNOBORDERONFLOATING)
        m_sWindowData.noBorder = CWindowOverridableVar(m_bIsFloating, PRIORITY_LAYOUT);
    else
        m_sWindowData.noBorder.unset(PRIORITY_LAYOUT);

    m_sWindowData.borderSize.matchOptional(workspaceRule.borderSize, PRIORITY_WORKSPACE_RULE);
    m_sWindowData.decorate.matchOptional(workspaceRule.decorate, PRIORITY_WORKSPACE_RULE);
    m_sWindowData.noBorder.matchOptional(workspaceRule.noBorder, PRIORITY_WORKSPACE_RULE);
    m_sWindowData.noRounding.matchOptional(workspaceRule.noRounding, PRIORITY_WORKSPACE_RULE);
    m_sWindowData.noShadow.matchOptional(workspaceRule.noShadow, PRIORITY_WORKSPACE_RULE);
}

int CWindow::getRealBorderSize() {
    if (m_sWindowData.noBorder.valueOrDefault() || (m_pWorkspace && isEffectiveInternalFSMode(FSMODE_FULLSCREEN)))
        return 0;

    static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");

    return m_sWindowData.borderSize.valueOr(*PBORDERSIZE);
}

float CWindow::getScrollMouse() {
    static auto PINPUTSCROLLFACTOR = CConfigValue<Hyprlang::FLOAT>("input:scroll_factor");
    return m_sWindowData.scrollMouse.valueOr(*PINPUTSCROLLFACTOR);
}

float CWindow::getScrollTouchpad() {
    static auto PTOUCHPADSCROLLFACTOR = CConfigValue<Hyprlang::FLOAT>("input:touchpad:scroll_factor");
    return m_sWindowData.scrollTouchpad.valueOr(*PTOUCHPADSCROLLFACTOR);
}

bool CWindow::canBeTorn() {
    static auto PTEARING = CConfigValue<Hyprlang::INT>("general:allow_tearing");
    return m_sWindowData.tearing.valueOr(m_bTearingHint) && *PTEARING;
}

void CWindow::setSuspended(bool suspend) {
    if (suspend == m_bSuspended)
        return;

    if (m_bIsX11 || !m_pXDGSurface->toplevel)
        return;

    m_pXDGSurface->toplevel->setSuspeneded(suspend);
    m_bSuspended = suspend;
}

bool CWindow::visibleOnMonitor(PHLMONITOR pMonitor) {
    CBox wbox = {m_vRealPosition->value(), m_vRealSize->value()};

    return !wbox.intersection({pMonitor->vecPosition, pMonitor->vecSize}).empty();
}

void CWindow::setAnimationsToMove() {
    m_vRealPosition->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsMove"));
    m_bAnimatingIn = false;
}

void CWindow::onWorkspaceAnimUpdate() {
    // clip box for animated offsets
    if (!m_bIsFloating || m_bPinned || isFullscreen()) {
        m_vFloatingOffset = Vector2D(0, 0);
        return;
    }

    Vector2D   offset;
    const auto PWORKSPACE = m_pWorkspace;
    if (!PWORKSPACE)
        return;

    const auto PWSMON = m_pMonitor.lock();
    if (!PWSMON)
        return;

    const auto WINBB = getFullWindowBoundingBox();
    if (PWORKSPACE->m_vRenderOffset->value().x != 0) {
        const auto PROGRESS = PWORKSPACE->m_vRenderOffset->value().x / PWSMON->vecSize.x;

        if (WINBB.x < PWSMON->vecPosition.x)
            offset.x += (PWSMON->vecPosition.x - WINBB.x) * PROGRESS;

        if (WINBB.x + WINBB.width > PWSMON->vecPosition.x + PWSMON->vecSize.x)
            offset.x += (WINBB.x + WINBB.width - PWSMON->vecPosition.x - PWSMON->vecSize.x) * PROGRESS;
    } else if (PWORKSPACE->m_vRenderOffset->value().y != 0) {
        const auto PROGRESS = PWORKSPACE->m_vRenderOffset->value().y / PWSMON->vecSize.y;

        if (WINBB.y < PWSMON->vecPosition.y)
            offset.y += (PWSMON->vecPosition.y - WINBB.y) * PROGRESS;

        if (WINBB.y + WINBB.height > PWSMON->vecPosition.y + PWSMON->vecSize.y)
            offset.y += (WINBB.y + WINBB.height - PWSMON->vecPosition.y - PWSMON->vecSize.y) * PROGRESS;
    }

    m_vFloatingOffset = offset;
}

void CWindow::onFocusAnimUpdate() {
    // borderangle once
    if (m_fBorderAngleAnimationProgress->enabled() && !m_fBorderAngleAnimationProgress->isBeingAnimated()) {
        m_fBorderAngleAnimationProgress->setValueAndWarp(0.f);
        *m_fBorderAngleAnimationProgress = 1.f;
    }
}

int CWindow::popupsCount() {
    if (m_bIsX11)
        return 0;

    int no = -1;
    m_pPopupHead->breadthfirst([](WP<CPopup> p, void* d) { *((int*)d) += 1; }, &no);
    return no;
}

int CWindow::surfacesCount() {
    if (m_bIsX11)
        return 1;

    int no = 0;
    m_pWLSurface->resource()->breadthfirst([](SP<CWLSurfaceResource> r, const Vector2D& offset, void* d) { *((int*)d) += 1; }, &no);
    return no;
}

void CWindow::clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize) {
    const Vector2D REALSIZE = m_vRealSize->goal();
    const Vector2D NEWSIZE  = REALSIZE.clamp(minSize.value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}), maxSize.value_or(Vector2D{INFINITY, INFINITY}));
    const Vector2D DELTA    = REALSIZE - NEWSIZE;

    *m_vRealPosition = m_vRealPosition->goal() + DELTA / 2.0;
    *m_vRealSize     = NEWSIZE;
    sendWindowSize(NEWSIZE);
}

bool CWindow::isFullscreen() {
    return m_sFullscreenState.internal != FSMODE_NONE;
}

bool CWindow::isEffectiveInternalFSMode(const eFullscreenMode MODE) {
    return (eFullscreenMode)std::bit_floor((uint8_t)m_sFullscreenState.internal) == MODE;
}

WORKSPACEID CWindow::workspaceID() {
    return m_pWorkspace ? m_pWorkspace->m_iID : m_iLastWorkspace;
}

MONITORID CWindow::monitorID() {
    return m_pMonitor ? m_pMonitor->ID : MONITOR_INVALID;
}

bool CWindow::onSpecialWorkspace() {
    return m_pWorkspace ? m_pWorkspace->m_bIsSpecialWorkspace : g_pCompositor->isWorkspaceSpecial(m_iLastWorkspace);
}

std::unordered_map<std::string, std::string> CWindow::getEnv() {

    const auto PID = getPID();

    if (PID <= 1)
        return {};

    std::unordered_map<std::string, std::string> results;

    //
    std::string   environFile = "/proc/" + std::to_string(PID) + "/environ";
    std::ifstream ifs(environFile, std::ios::binary);

    if (!ifs.good())
        return {};

    std::vector<char> buffer;
    size_t            needle = 0;
    buffer.resize(512, '\0');
    while (ifs.read(buffer.data() + needle, 512)) {
        buffer.resize(buffer.size() + 512, '\0');
        needle += 512;
    }

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
    if (g_pCompositor->m_pLastWindow == m_pSelf)
        return;

    static auto PFOCUSONACTIVATE = CConfigValue<Hyprlang::INT>("misc:focus_on_activate");

    g_pEventManager->postEvent(SHyprIPCEvent{"urgent", std::format("{:x}", (uintptr_t)this)});
    EMIT_HOOK_EVENT("urgent", m_pSelf.lock());

    m_bIsUrgent = true;

    if (!force && (!m_sWindowData.focusOnActivate.valueOr(*PFOCUSONACTIVATE) || (m_eSuppressedEvents & SUPPRESS_ACTIVATE_FOCUSONLY) || (m_eSuppressedEvents & SUPPRESS_ACTIVATE)))
        return;

    if (!m_bIsMapped) {
        Debug::log(LOG, "Ignoring CWindow::activate focus/warp, window is not mapped yet.");
        return;
    }

    if (m_bIsFloating)
        g_pCompositor->changeWindowZOrder(m_pSelf.lock(), true);

    g_pCompositor->focusWindow(m_pSelf.lock());
    warpCursor();
}

void CWindow::onUpdateState() {
    std::optional<bool>      requestsFS = m_pXDGSurface ? m_pXDGSurface->toplevel->state.requestsFullscreen : m_pXWaylandSurface->state.requestsFullscreen;
    std::optional<MONITORID> requestsID = m_pXDGSurface ? m_pXDGSurface->toplevel->state.requestsFullscreenMonitor : MONITOR_INVALID;
    std::optional<bool>      requestsMX = m_pXDGSurface ? m_pXDGSurface->toplevel->state.requestsMaximize : m_pXWaylandSurface->state.requestsMaximize;

    if (requestsFS.has_value() && !(m_eSuppressedEvents & SUPPRESS_FULLSCREEN)) {
        if (requestsID.has_value() && (requestsID.value() != MONITOR_INVALID) && !(m_eSuppressedEvents & SUPPRESS_FULLSCREEN_OUTPUT)) {
            if (m_bIsMapped) {
                const auto monitor = g_pCompositor->getMonitorFromID(requestsID.value());
                g_pCompositor->moveWindowToWorkspaceSafe(m_pSelf.lock(), monitor->activeWorkspace);
                g_pCompositor->setActiveMonitor(monitor);
            }

            if (!m_bIsMapped)
                m_iWantsInitialFullscreenMonitor = requestsID.value();
        }

        bool fs = requestsFS.value();
        if (m_bIsMapped)
            g_pCompositor->changeWindowFullscreenModeClient(m_pSelf.lock(), FSMODE_FULLSCREEN, requestsFS.value());

        if (!m_bIsMapped)
            m_bWantsInitialFullscreen = fs;
    }

    if (requestsMX.has_value() && !(m_eSuppressedEvents & SUPPRESS_MAXIMIZE)) {
        if (m_bIsMapped)
            g_pCompositor->changeWindowFullscreenModeClient(m_pSelf.lock(), FSMODE_MAXIMIZED, requestsMX.value());
    }
}

void CWindow::onUpdateMeta() {
    const auto NEWTITLE = fetchTitle();
    bool       doUpdate = false;

    if (m_szTitle != NEWTITLE) {
        m_szTitle = NEWTITLE;
        g_pEventManager->postEvent(SHyprIPCEvent{"windowtitle", std::format("{:x}", (uintptr_t)this)});
        g_pEventManager->postEvent(SHyprIPCEvent{"windowtitlev2", std::format("{:x},{}", (uintptr_t)this, m_szTitle)});
        EMIT_HOOK_EVENT("windowTitle", m_pSelf.lock());

        if (m_pSelf == g_pCompositor->m_pLastWindow) { // if it's the active, let's post an event to update others
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", m_szClass + "," + m_szTitle});
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", std::format("{:x}", (uintptr_t)this)});
            EMIT_HOOK_EVENT("activeWindow", m_pSelf.lock());
        }

        Debug::log(LOG, "Window {:x} set title to {}", (uintptr_t)this, m_szTitle);
        doUpdate = true;
    }

    const auto NEWCLASS = fetchClass();
    if (m_szClass != NEWCLASS) {
        m_szClass = NEWCLASS;

        if (m_pSelf == g_pCompositor->m_pLastWindow) { // if it's the active, let's post an event to update others
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", m_szClass + "," + m_szTitle});
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", std::format("{:x}", (uintptr_t)this)});
            EMIT_HOOK_EVENT("activeWindow", m_pSelf.lock());
        }

        Debug::log(LOG, "Window {:x} set class to {}", (uintptr_t)this, m_szClass);
        doUpdate = true;
    }

    if (doUpdate) {
        updateDynamicRules();
        g_pCompositor->updateWindowAnimatedDecorationValues(m_pSelf.lock());
        updateToplevel();
    }
}

std::string CWindow::fetchTitle() {
    if (!m_bIsX11) {
        if (m_pXDGSurface && m_pXDGSurface->toplevel)
            return m_pXDGSurface->toplevel->state.title;
    } else {
        if (m_pXWaylandSurface)
            return m_pXWaylandSurface->state.title;
    }

    return "";
}

std::string CWindow::fetchClass() {
    if (!m_bIsX11) {
        if (m_pXDGSurface && m_pXDGSurface->toplevel)
            return m_pXDGSurface->toplevel->state.appid;
    } else {
        if (m_pXWaylandSurface)
            return m_pXWaylandSurface->state.appid;
    }

    return "";
}

void CWindow::onAck(uint32_t serial) {
    const auto SERIAL = std::find_if(m_vPendingSizeAcks.rbegin(), m_vPendingSizeAcks.rend(), [serial](const auto& e) { return e.first == serial; });

    if (SERIAL == m_vPendingSizeAcks.rend())
        return;

    m_pPendingSizeAck = *SERIAL;
    std::erase_if(m_vPendingSizeAcks, [&](const auto& el) { return el.first <= SERIAL->first; });
}

void CWindow::onResourceChangeX11() {
    if (m_pXWaylandSurface->surface && !m_pWLSurface->resource())
        m_pWLSurface->assign(m_pXWaylandSurface->surface.lock(), m_pSelf.lock());
    else if (!m_pXWaylandSurface->surface && m_pWLSurface->resource())
        m_pWLSurface->unassign();

    // update metadata as well,
    // could be first assoc and we need to catch the class
    onUpdateMeta();

    Debug::log(LOG, "xwayland window {:x} -> association to {:x}", (uintptr_t)m_pXWaylandSurface.get(), (uintptr_t)m_pWLSurface->resource().get());
}

void CWindow::onX11Configure(CBox box) {

    if (!m_pXWaylandSurface->surface || !m_pXWaylandSurface->mapped || !m_bIsMapped) {
        m_pXWaylandSurface->configure(box);
        m_vPendingReportedSize = box.size();
        m_vReportedSize        = box.size();
        if (const auto PMONITOR = m_pMonitor.lock(); PMONITOR)
            m_fX11SurfaceScaledBy = PMONITOR->scale;
        return;
    }

    g_pHyprRenderer->damageWindow(m_pSelf.lock());

    if (!m_bIsFloating || isFullscreen() || g_pInputManager->currentlyDraggedWindow == m_pSelf) {
        sendWindowSize(m_vRealSize->goal(), true);
        g_pInputManager->refocus();
        g_pHyprRenderer->damageWindow(m_pSelf.lock());
        return;
    }

    if (box.size() > Vector2D{1, 1})
        setHidden(false);
    else
        setHidden(true);

    const auto LOGICALPOS = g_pXWaylandManager->xwaylandToWaylandCoords(box.pos());

    m_vRealPosition->setValueAndWarp(LOGICALPOS);
    m_vRealSize->setValueAndWarp(box.size());

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");
    if (*PXWLFORCESCALEZERO) {
        if (const auto PMONITOR = m_pMonitor.lock(); PMONITOR) {
            m_vRealSize->setValueAndWarp(m_vRealSize->goal() / PMONITOR->scale);
            m_fX11SurfaceScaledBy = PMONITOR->scale;
        }
    }

    m_vPosition = m_vRealPosition->goal();
    m_vSize     = m_vRealSize->goal();

    sendWindowSize(box.size(), true);

    m_vPendingReportedSize = box.size();
    m_vReportedSize        = box.size();

    updateWindowDecos();

    if (!m_pWorkspace || !m_pWorkspace->isVisible())
        return; // further things are only for visible windows

    m_pWorkspace = g_pCompositor->getMonitorFromVector(m_vRealPosition->goal() + m_vRealSize->goal() / 2.f)->activeWorkspace;

    g_pCompositor->changeWindowZOrder(m_pSelf.lock(), true);

    m_bCreatedOverFullscreen = true;

    g_pHyprRenderer->damageWindow(m_pSelf.lock());
}

void CWindow::warpCursor(bool force) {
    static auto PERSISTENTWARPS         = CConfigValue<Hyprlang::INT>("cursor:persistent_warps");
    const auto  coords                  = m_vRelativeCursorCoordsOnLastWarp;
    m_vRelativeCursorCoordsOnLastWarp.x = -1; // reset m_vRelativeCursorCoordsOnLastWarp

    if (*PERSISTENTWARPS && coords.x > 0 && coords.y > 0 && coords < m_vSize) // don't warp cursor outside the window
        g_pCompositor->warpCursorTo(m_vPosition + coords, force);
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

        for (auto const& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped || w->isHidden())
                continue;

            if (w->getPID() == currentPid)
                candidates.push_back(w);
        }
    }

    if (!(*PSWALLOWREGEX).empty())
        std::erase_if(candidates, [&](const auto& other) { return !RE2::FullMatch(other->m_szClass, *PSWALLOWREGEX); });

    if (candidates.size() <= 0)
        return nullptr;

    if (!(*PSWALLOWEXREGEX).empty())
        std::erase_if(candidates, [&](const auto& other) { return RE2::FullMatch(other->m_szTitle, *PSWALLOWEXREGEX); });

    if (candidates.size() <= 0)
        return nullptr;

    if (candidates.size() == 1)
        return candidates.at(0);

    // walk up the focus history and find the last focused
    for (auto const& w : g_pCompositor->m_vWindowFocusHistory) {
        if (!w)
            continue;

        if (std::find(candidates.begin(), candidates.end(), w.lock()) != candidates.end())
            return w.lock();
    }

    // if none are found (??) then just return the first one
    return candidates.at(0);
}

void CWindow::unsetWindowData(eOverridePriority priority) {
    for (auto const& element : g_pConfigManager->mbWindowProperties) {
        element.second(m_pSelf.lock())->unset(priority);
    }
    for (auto const& element : g_pConfigManager->miWindowProperties) {
        element.second(m_pSelf.lock())->unset(priority);
    }
    for (auto const& element : g_pConfigManager->mfWindowProperties) {
        element.second(m_pSelf.lock())->unset(priority);
    }
}

bool CWindow::isX11OverrideRedirect() {
    return m_pXWaylandSurface && m_pXWaylandSurface->overrideRedirect;
}

bool CWindow::isModal() {
    return (m_pXWaylandSurface && m_pXWaylandSurface->modal);
}

Vector2D CWindow::requestedMinSize() {
    if ((m_bIsX11 && !m_pXWaylandSurface->sizeHints) || (!m_bIsX11 && !m_pXDGSurface->toplevel))
        return Vector2D(1, 1);

    Vector2D minSize = m_bIsX11 ? Vector2D(m_pXWaylandSurface->sizeHints->min_width, m_pXWaylandSurface->sizeHints->min_height) : m_pXDGSurface->toplevel->layoutMinSize();

    minSize = minSize.clamp({1, 1});

    return minSize;
}

Vector2D CWindow::requestedMaxSize() {
    constexpr int NO_MAX_SIZE_LIMIT = 99999;
    if (((m_bIsX11 && !m_pXWaylandSurface->sizeHints) || (!m_bIsX11 && !m_pXDGSurface->toplevel) || m_sWindowData.noMaxSize.valueOrDefault()))
        return Vector2D(NO_MAX_SIZE_LIMIT, NO_MAX_SIZE_LIMIT);

    Vector2D maxSize = m_bIsX11 ? Vector2D(m_pXWaylandSurface->sizeHints->max_width, m_pXWaylandSurface->sizeHints->max_height) : m_pXDGSurface->toplevel->layoutMaxSize();

    if (maxSize.x < 5)
        maxSize.x = NO_MAX_SIZE_LIMIT;
    if (maxSize.y < 5)
        maxSize.y = NO_MAX_SIZE_LIMIT;

    return maxSize;
}

void CWindow::sendWindowSize(Vector2D size, bool force, std::optional<Vector2D> overridePos) {
    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");
    const auto  PMONITOR           = m_pMonitor.lock();

    size = size.clamp(Vector2D{1, 1}, Vector2D{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()});

    // calculate pos
    // TODO: this should be decoupled from setWindowSize IMO
    Vector2D windowPos = overridePos.value_or(m_vRealPosition->goal());

    if (m_bIsX11 && PMONITOR) {
        windowPos = g_pXWaylandManager->waylandToXWaylandCoords(windowPos);
        if (*PXWLFORCESCALEZERO)
            size *= PMONITOR->scale;
    }

    if (!force && m_vPendingReportedSize == size && (windowPos == m_vReportedPosition || !m_bIsX11))
        return;

    m_vReportedPosition    = windowPos;
    m_vPendingReportedSize = size;
    m_fX11SurfaceScaledBy  = 1.0f;

    if (*PXWLFORCESCALEZERO && m_bIsX11 && PMONITOR)
        m_fX11SurfaceScaledBy = PMONITOR->scale;

    if (m_bIsX11 && m_pXWaylandSurface)
        m_pXWaylandSurface->configure({windowPos, size});
    else if (m_pXDGSurface && m_pXDGSurface->toplevel)
        m_vPendingSizeAcks.emplace_back(m_pXDGSurface->toplevel->setSize(size), size.floor());
}

NContentType::eContentType CWindow::getContentType() {
    if (!m_pWLSurface || !m_pWLSurface->resource() || !m_pWLSurface->resource()->contentType.valid())
        return CONTENT_TYPE_NONE;

    return m_pWLSurface->resource()->contentType->value;
}

void CWindow::setContentType(NContentType::eContentType contentType) {
    if (!m_pWLSurface->resource()->contentType.valid())
        m_pWLSurface->resource()->contentType = PROTO::contentType->getContentType(m_pWLSurface->resource());
    // else disallow content type change if proto is used?

    Debug::log(INFO, "ContentType for window {}", (int)contentType);
    m_pWLSurface->resource()->contentType->value = contentType;
}
