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
    } else if (PWINDOW->m_pXDGSurface && PWINDOW->m_pXDGSurface->toplevel)
        PWINDOW->m_pXDGSurface->toplevel->setActive(activate);
}

void CHyprXWaylandManager::activateWindow(PHLWINDOW pWindow, bool activate) {
    if (pWindow->m_bIsX11) {

        if (activate) {
            pWindow->sendWindowSize(true); // update xwayland output pos
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

CBox CHyprXWaylandManager::getGeometryForWindow(PHLWINDOW pWindow) {
    if (!pWindow)
        return {};

    CBox box;

    if (pWindow->m_bIsX11)
        box = pWindow->m_pXWaylandSurface->geometry;
    else if (pWindow->m_pXDGSurface)
        box = pWindow->m_pXDGSurface->current.geometry;

    return box;
}

void CHyprXWaylandManager::sendCloseWindow(PHLWINDOW pWindow) {
    if (pWindow->m_bIsX11)
        pWindow->m_pXWaylandSurface->close();
    else if (pWindow->m_pXDGSurface && pWindow->m_pXDGSurface->toplevel)
        pWindow->m_pXDGSurface->toplevel->close();
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

                if (a != HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"])
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
