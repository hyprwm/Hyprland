#include "XWaylandManager.hpp"
#include "../Compositor.hpp"
#include "../events/Events.hpp"
#include "../config/ConfigValue.hpp"
#include "../helpers/Monitor.hpp"
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
    return pWindow ? pWindow->m_wlSurface->resource() : nullptr;
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

    if (PWINDOW->m_isX11) {
        if (PWINDOW->m_xwaylandSurface) {
            if (activate) {
                PWINDOW->m_xwaylandSurface->setMinimized(false);
                PWINDOW->m_xwaylandSurface->restackToTop();
            }
            PWINDOW->m_xwaylandSurface->activate(activate);
        }
    } else if (PWINDOW->m_xdgSurface && PWINDOW->m_xdgSurface->toplevel)
        PWINDOW->m_xdgSurface->toplevel->setActive(activate);
}

void CHyprXWaylandManager::activateWindow(PHLWINDOW pWindow, bool activate) {
    if (pWindow->m_isX11) {

        if (activate) {
            pWindow->sendWindowSize(true); // update xwayland output pos
            pWindow->m_xwaylandSurface->setMinimized(false);

            if (!pWindow->isX11OverrideRedirect())
                pWindow->m_xwaylandSurface->restackToTop();
        }

        pWindow->m_xwaylandSurface->activate(activate);

    } else if (pWindow->m_xdgSurface && pWindow->m_xdgSurface->toplevel)
        pWindow->m_xdgSurface->toplevel->setActive(activate);

    if (activate) {
        g_pCompositor->m_lastFocus  = getWindowSurface(pWindow);
        g_pCompositor->m_lastWindow = pWindow;
    }

    if (!pWindow->m_pinned)
        pWindow->m_workspace->m_lastFocusedWindow = pWindow;
}

CBox CHyprXWaylandManager::getGeometryForWindow(PHLWINDOW pWindow) {
    if (!pWindow)
        return {};

    CBox box;

    if (pWindow->m_isX11)
        box = pWindow->m_xwaylandSurface->geometry;
    else if (pWindow->m_xdgSurface)
        box = pWindow->m_xdgSurface->current.geometry;

    return box;
}

void CHyprXWaylandManager::sendCloseWindow(PHLWINDOW pWindow) {
    if (pWindow->m_isX11)
        pWindow->m_xwaylandSurface->close();
    else if (pWindow->m_xdgSurface && pWindow->m_xdgSurface->toplevel)
        pWindow->m_xdgSurface->toplevel->close();
}

bool CHyprXWaylandManager::shouldBeFloated(PHLWINDOW pWindow, bool pending) {
    if (pWindow->m_isX11) {
        for (const auto& a : pWindow->m_xwaylandSurface->atoms)
            if (a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLBAR"] ||
                a == HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] ||
                a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] ||
                a == HYPRATOMS["_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"]) {

                if (a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"])
                    pWindow->m_X11ShouldntFocus = true;

                if (a != HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"])
                    pWindow->m_noInitialFocus = true;

                return true;
            }

        if (pWindow->isModal() || pWindow->m_xwaylandSurface->transient ||
            (pWindow->m_xwaylandSurface->role.contains("task_dialog") || pWindow->m_xwaylandSurface->role.contains("pop-up")) || pWindow->m_xwaylandSurface->overrideRedirect)
            return true;

        const auto SIZEHINTS = pWindow->m_xwaylandSurface->sizeHints.get();
        if (pWindow->m_xwaylandSurface->transient || pWindow->m_xwaylandSurface->parent ||
            (SIZEHINTS && (SIZEHINTS->min_width == SIZEHINTS->max_width) && (SIZEHINTS->min_height == SIZEHINTS->max_height)))
            return true;
    } else {
        if (!pWindow->m_xdgSurface || !pWindow->m_xdgSurface->toplevel)
            return false;

        const auto PSTATE = pending ? &pWindow->m_xdgSurface->toplevel->pending : &pWindow->m_xdgSurface->toplevel->current;
        if (pWindow->m_xdgSurface->toplevel->parent ||
            (PSTATE->minSize.x != 0 && PSTATE->minSize.y != 0 && (PSTATE->minSize.x == PSTATE->maxSize.x || PSTATE->minSize.y == PSTATE->maxSize.y)))
            return true;
    }

    return false;
}

void CHyprXWaylandManager::checkBorders(PHLWINDOW pWindow) {
    if (!pWindow->m_isX11)
        return;

    for (auto const& a : pWindow->m_xwaylandSurface->atoms) {
        if (a == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
            a == HYPRATOMS["_NET_WM_WINDOW_TYPE_COMBO"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] || a == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
            a == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"]) {

            pWindow->m_X11DoesntWantBorders = true;
            return;
        }
    }

    if (pWindow->isX11OverrideRedirect())
        pWindow->m_X11DoesntWantBorders = true;
}

void CHyprXWaylandManager::setWindowFullscreen(PHLWINDOW pWindow, bool fullscreen) {
    if (!pWindow)
        return;

    if (pWindow->m_isX11)
        pWindow->m_xwaylandSurface->setFullscreen(fullscreen);
    else if (pWindow->m_xdgSurface && pWindow->m_xdgSurface->toplevel)
        pWindow->m_xdgSurface->toplevel->setFullscreen(fullscreen);
}

Vector2D CHyprXWaylandManager::waylandToXWaylandCoords(const Vector2D& coord) {
    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    PHLMONITOR  pMonitor     = nullptr;
    double      bestDistance = __FLT_MAX__;
    for (const auto& m : g_pCompositor->m_monitors) {
        const auto SIZ = *PXWLFORCESCALEZERO ? m->m_transformedSize : m->m_size;

        double     distance = vecToRectDistanceSquared(coord, {m->m_position.x, m->m_position.y}, {m->m_position.x + SIZ.x - 1, m->m_position.y + SIZ.y - 1});

        if (distance < bestDistance) {
            bestDistance = distance;
            pMonitor     = m;
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->m_position;
    // if scaled, scale
    if (*PXWLFORCESCALEZERO)
        result *= pMonitor->m_scale;
    // add pos
    result += pMonitor->m_xWaylandPosition;

    return result;
}

Vector2D CHyprXWaylandManager::xwaylandToWaylandCoords(const Vector2D& coord) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    PHLMONITOR  pMonitor     = nullptr;
    double      bestDistance = __FLT_MAX__;
    for (const auto& m : g_pCompositor->m_monitors) {
        const auto SIZ = *PXWLFORCESCALEZERO ? m->m_transformedSize : m->m_size;

        double     distance =
            vecToRectDistanceSquared(coord, {m->m_xWaylandPosition.x, m->m_xWaylandPosition.y}, {m->m_xWaylandPosition.x + SIZ.x - 1, m->m_xWaylandPosition.y + SIZ.y - 1});

        if (distance < bestDistance) {
            bestDistance = distance;
            pMonitor     = m;
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->m_xWaylandPosition;
    // if scaled, unscale
    if (*PXWLFORCESCALEZERO)
        result /= pMonitor->m_scale;
    // add pos
    result += pMonitor->m_position;

    return result;
}
