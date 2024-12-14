#include "XWaylandManager.hpp"
#include "../Compositor.hpp"
#include "../events/Events.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../xwayland/XWayland.hpp"
#include <hyprutils/math/Vector2D.hpp>

#define OUTPUT_MANAGER_VERSION                   3
#define OUTPUT_DONE_DEPRECATED_SINCE_VERSION     3
#define OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

CHyprXWaylandManager::CHyprXWaylandManager() = default;

CHyprXWaylandManager::~CHyprXWaylandManager() {
#ifndef NO_XWAYLAND
    unsetenv("DISPLAY");
#endif
}

SP<CWLSurfaceResource> CHyprXWaylandManager::getWindowSurface(PHLWINDOW pWindow) {
    return pWindow ? pWindow->m_pWLSurface->resource() : nullptr;
}

void CHyprXWaylandManager::activateSurface(SP<CWLSurfaceResource> pSurface, bool activate) {
    if (!pSurface)
        return;

    auto HLSurface = CWLSurface::fromResource(pSurface);
    if (!HLSurface) {
        Debug::log(TRACE, "CHyprXWaylandManager::activateSurface on non-desktop surface, ignoring");
        return;
    }

    const auto PWINDOW = HLSurface->getWindow();
    if (!PWINDOW) {
        Debug::log(TRACE, "CHyprXWaylandManager::activateSurface on non-window surface, ignoring");
        return;
    }

    if (PWINDOW->m_bIsX11) {
        if (PWINDOW->m_pXWaylandSurface) {
            if (activate) {
                PWINDOW->m_pXWaylandSurface->setMinimized(false);
                PWINDOW->m_pXWaylandSurface->restackToTop();
            }
            PWINDOW->m_pXWaylandSurface->activate(activate);
        }
    } else if (PWINDOW->m_pXDGSurface)
        PWINDOW->m_pXDGSurface->toplevel->setActive(activate);
}

void CHyprXWaylandManager::activateWindow(PHLWINDOW pWindow, bool activate) {
    if (pWindow->m_bIsX11) {

        if (activate) {
            setWindowSize(pWindow, pWindow->m_vRealSize.value()); // update xwayland output pos
            pWindow->m_pXWaylandSurface->setMinimized(false);

            if (!pWindow->isX11OverrideRedirect())
                pWindow->m_pXWaylandSurface->restackToTop();
        }

        pWindow->m_pXWaylandSurface->activate(activate);

    } else if (pWindow->m_pXDGSurface && pWindow->m_pXDGSurface->toplevel)
        pWindow->m_pXDGSurface->toplevel->setActive(activate);

    if (activate) {
        g_pCompositor->m_pLastFocus  = getWindowSurface(pWindow);
        g_pCompositor->m_pLastWindow = pWindow;
    }

    if (!pWindow->m_bPinned)
        pWindow->m_pWorkspace->m_pLastFocusedWindow = pWindow;
}

void CHyprXWaylandManager::getGeometryForWindow(PHLWINDOW pWindow, CBox* pbox) {
    if (!pWindow)
        return;

    if (pWindow->m_bIsX11) {
        const auto SIZEHINTS = pWindow->m_pXWaylandSurface->sizeHints.get();

        if (SIZEHINTS && !pWindow->isX11OverrideRedirect()) {
            // WM_SIZE_HINTS' x,y,w,h is deprecated it seems.
            // Source: https://x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html#wm_normal_hints_property
            pbox->x = pWindow->m_pXWaylandSurface->geometry.x;
            pbox->y = pWindow->m_pXWaylandSurface->geometry.y;

            constexpr int ICCCM_USSize = 0x2;
            constexpr int ICCCM_PSize  = 0x8;

            if ((SIZEHINTS->flags & ICCCM_USSize) || (SIZEHINTS->flags & ICCCM_PSize)) {
                pbox->w = SIZEHINTS->base_width;
                pbox->h = SIZEHINTS->base_height;
            } else {
                pbox->w = pWindow->m_pXWaylandSurface->geometry.w;
                pbox->h = pWindow->m_pXWaylandSurface->geometry.h;
            }
        } else
            *pbox = pWindow->m_pXWaylandSurface->geometry;

    } else if (pWindow->m_pXDGSurface)
        *pbox = pWindow->m_pXDGSurface->current.geometry;
}

void CHyprXWaylandManager::sendCloseWindow(PHLWINDOW pWindow) {
    if (pWindow->m_bIsX11)
        pWindow->m_pXWaylandSurface->close();
    else if (pWindow->m_pXDGSurface && pWindow->m_pXDGSurface->toplevel)
        pWindow->m_pXDGSurface->toplevel->close();
}

void CHyprXWaylandManager::setWindowSize(PHLWINDOW pWindow, Vector2D size, bool force) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  PMONITOR = pWindow->m_pMonitor.lock();

    size = size.clamp(Vector2D{0, 0}, Vector2D{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()});

    // calculate pos
    // TODO: this should be decoupled from setWindowSize IMO
    Vector2D windowPos = pWindow->m_vRealPosition.value();

    if (pWindow->m_bIsX11 && PMONITOR) {
        windowPos -= PMONITOR->vecPosition; // normalize to monitor
        if (*PXWLFORCESCALEZERO)
            windowPos *= PMONITOR->scale;           // scale if applicable
        windowPos += PMONITOR->vecXWaylandPosition; // move to correct position for xwayland
    }

    if (!force && pWindow->m_vPendingReportedSize == size && (windowPos == pWindow->m_vReportedPosition || !pWindow->m_bIsX11))
        return;

    pWindow->m_vReportedPosition    = windowPos;
    pWindow->m_vPendingReportedSize = size;

    pWindow->m_fX11SurfaceScaledBy = 1.0f;

    if (*PXWLFORCESCALEZERO && pWindow->m_bIsX11 && PMONITOR) {
        size *= PMONITOR->scale;
        pWindow->m_fX11SurfaceScaledBy = PMONITOR->scale;
    }

    if (pWindow->m_bIsX11)
        pWindow->m_pXWaylandSurface->configure({windowPos, size});
    else if (pWindow->m_pXDGSurface->toplevel)
        pWindow->m_vPendingSizeAcks.emplace_back(pWindow->m_pXDGSurface->toplevel->setSize(size), size.floor());
}

bool CHyprXWaylandManager::shouldBeFloated(PHLWINDOW pWindow, bool pending) {
    if (pWindow->m_bIsX11) {
        for (const auto& a : pWindow->m_pXWaylandSurface->atoms)
            if (a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLBAR"] ||
                a == HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] ||
                a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] ||
                a == HYPRATOMS["_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"]) {

                if (a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"])
                    pWindow->m_bX11ShouldntFocus = true;

                pWindow->m_bNoInitialFocus = true;
                return true;
            }

        if (pWindow->isModal() || pWindow->m_pXWaylandSurface->transient ||
            (pWindow->m_pXWaylandSurface->role.contains("task_dialog") || pWindow->m_pXWaylandSurface->role.contains("pop-up")) || pWindow->m_pXWaylandSurface->overrideRedirect)
            return true;

        const auto SIZEHINTS = pWindow->m_pXWaylandSurface->sizeHints.get();
        if (pWindow->m_pXWaylandSurface->transient || pWindow->m_pXWaylandSurface->parent ||
            (SIZEHINTS && (SIZEHINTS->min_width == SIZEHINTS->max_width) && (SIZEHINTS->min_height == SIZEHINTS->max_height)))
            return true;
    } else {
        const auto PSTATE = pending ? &pWindow->m_pXDGSurface->toplevel->pending : &pWindow->m_pXDGSurface->toplevel->current;

        if (pWindow->m_pXDGSurface->toplevel->parent ||
            (PSTATE->minSize.x != 0 && PSTATE->minSize.y != 0 && (PSTATE->minSize.x == PSTATE->maxSize.x || PSTATE->minSize.y == PSTATE->maxSize.y)))
            return true;
    }

    return false;
}

void CHyprXWaylandManager::checkBorders(PHLWINDOW pWindow) {
    if (!pWindow->m_bIsX11)
        return;

    for (auto const& a : pWindow->m_pXWaylandSurface->atoms) {
        if (a == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
            a == HYPRATOMS["_NET_WM_WINDOW_TYPE_COMBO"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
            a == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"]) {

            pWindow->m_bX11DoesntWantBorders = true;
            return;
        }
    }

    if (pWindow->isX11OverrideRedirect())
        pWindow->m_bX11DoesntWantBorders = true;
}

void CHyprXWaylandManager::setWindowFullscreen(PHLWINDOW pWindow, bool fullscreen) {
    if (!pWindow)
        return;

    if (pWindow->m_bIsX11)
        pWindow->m_pXWaylandSurface->setFullscreen(fullscreen);
    else if (pWindow->m_pXDGSurface && pWindow->m_pXDGSurface->toplevel)
        pWindow->m_pXDGSurface->toplevel->setFullscreen(fullscreen);
}

Vector2D CHyprXWaylandManager::waylandToXWaylandCoords(const Vector2D& coord) {
    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    PHLMONITOR  pMonitor     = nullptr;
    double      bestDistance = __FLT_MAX__;
    for (const auto& m : g_pCompositor->m_vMonitors) {
        const auto SIZ = *PXWLFORCESCALEZERO ? m->vecTransformedSize : m->vecSize;

        double     distance = vecToRectDistanceSquared(coord, {m->vecPosition.x, m->vecPosition.y}, {m->vecPosition.x + SIZ.x - 1, m->vecPosition.y + SIZ.y - 1});

        if (distance < bestDistance) {
            bestDistance = distance;
            pMonitor     = m;
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->vecPosition;
    // if scaled, scale
    if (*PXWLFORCESCALEZERO)
        result *= pMonitor->scale;
    // add pos
    result += pMonitor->vecXWaylandPosition;

    return result;
}

Vector2D CHyprXWaylandManager::xwaylandToWaylandCoords(const Vector2D& coord) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    PHLMONITOR  pMonitor     = nullptr;
    double      bestDistance = __FLT_MAX__;
    for (const auto& m : g_pCompositor->m_vMonitors) {
        const auto SIZ = *PXWLFORCESCALEZERO ? m->vecTransformedSize : m->vecSize;

        double     distance =
            vecToRectDistanceSquared(coord, {m->vecXWaylandPosition.x, m->vecXWaylandPosition.y}, {m->vecXWaylandPosition.x + SIZ.x - 1, m->vecXWaylandPosition.y + SIZ.y - 1});

        if (distance < bestDistance) {
            bestDistance = distance;
            pMonitor     = m;
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->vecXWaylandPosition;
    // if scaled, unscale
    if (*PXWLFORCESCALEZERO)
        result /= pMonitor->scale;
    // add pos
    result += pMonitor->vecPosition;

    return result;
}