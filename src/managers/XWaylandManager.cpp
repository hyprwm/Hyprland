#include "XWaylandManager.hpp"
#include "../Compositor.hpp"
#include "../events/Events.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/XDGShell.hpp"

#define OUTPUT_MANAGER_VERSION                   3
#define OUTPUT_DONE_DEPRECATED_SINCE_VERSION     3
#define OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

CHyprXWaylandManager::CHyprXWaylandManager() {
#ifndef NO_XWAYLAND
    m_sWLRXWayland = wlr_xwayland_create(g_pCompositor->m_sWLDisplay, g_pCompositor->m_sWLRCompositor, 1);

    if (!m_sWLRXWayland) {
        Debug::log(ERR, "Couldn't start up the XWaylandManager because wlr_xwayland_create returned a nullptr!");
        return;
    }

    addWLSignal(&m_sWLRXWayland->events.ready, &Events::listen_readyXWayland, m_sWLRXWayland, "XWayland Manager");
    addWLSignal(&m_sWLRXWayland->events.new_surface, &Events::listen_surfaceXWayland, m_sWLRXWayland, "XWayland Manager");

    setenv("DISPLAY", m_sWLRXWayland->display_name, 1);

    Debug::log(LOG, "CHyprXWaylandManager started on display {}", m_sWLRXWayland->display_name);
#else
    unsetenv("DISPLAY"); // unset DISPLAY so that X11 apps do not try to start on a different/invalid DISPLAY
#endif
}

CHyprXWaylandManager::~CHyprXWaylandManager() {
#ifndef NO_XWAYLAND
    unsetenv("DISPLAY");
#endif
}

wlr_surface* CHyprXWaylandManager::getWindowSurface(PHLWINDOW pWindow) {
    return pWindow->m_pWLSurface.wlr();
}

void CHyprXWaylandManager::activateSurface(wlr_surface* pSurface, bool activate) {
    if (!pSurface)
        return;

    if (wlr_xwayland_surface_try_from_wlr_surface(pSurface)) {
        const auto XSURF = wlr_xwayland_surface_try_from_wlr_surface(pSurface);
        wlr_xwayland_surface_activate(XSURF, activate);

        if (activate && !XSURF->override_redirect)
            wlr_xwayland_surface_restack(XSURF, nullptr, XCB_STACK_MODE_ABOVE);
    }

    // TODO:
    // this cannot be nicely done until we rewrite wlr_surface
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsX11 || !w->m_bIsMapped)
            continue;

        if (w->m_pWLSurface.wlr() != pSurface)
            continue;

        w->m_pXDGSurface->toplevel->setActive(activate);
    }
}

void CHyprXWaylandManager::activateWindow(PHLWINDOW pWindow, bool activate) {
    if (pWindow->m_bIsX11) {
        setWindowSize(pWindow, pWindow->m_vRealSize.value()); // update xwayland output pos

        if (activate) {
            wlr_xwayland_surface_set_minimized(pWindow->m_uSurface.xwayland, false);
            if (!pWindow->m_uSurface.xwayland->override_redirect)
                wlr_xwayland_surface_restack(pWindow->m_uSurface.xwayland, nullptr, XCB_STACK_MODE_ABOVE);
        }

        wlr_xwayland_surface_activate(pWindow->m_uSurface.xwayland, activate);
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
    if (pWindow->m_bIsX11) {
        const auto SIZEHINTS = pWindow->m_uSurface.xwayland->size_hints;

        if (SIZEHINTS && pWindow->m_iX11Type != 2) {
            pbox->x      = SIZEHINTS->x;
            pbox->y      = SIZEHINTS->y;
            pbox->width  = SIZEHINTS->width;
            pbox->height = SIZEHINTS->height;
        } else {
            pbox->x      = pWindow->m_uSurface.xwayland->x;
            pbox->y      = pWindow->m_uSurface.xwayland->y;
            pbox->width  = pWindow->m_uSurface.xwayland->width;
            pbox->height = pWindow->m_uSurface.xwayland->height;
        }
    } else if (pWindow->m_pXDGSurface)
        *pbox = pWindow->m_pXDGSurface->current.geometry;
}

void CHyprXWaylandManager::sendCloseWindow(PHLWINDOW pWindow) {
    if (pWindow->m_bIsX11)
        wlr_xwayland_surface_close(pWindow->m_uSurface.xwayland);
    else if (pWindow->m_pXDGSurface && pWindow->m_pXDGSurface->toplevel)
        pWindow->m_pXDGSurface->toplevel->close();
}

void CHyprXWaylandManager::setWindowSize(PHLWINDOW pWindow, Vector2D size, bool force) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    size = size.clamp(Vector2D{0, 0}, Vector2D{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()});

    // calculate pos
    // TODO: this should be decoupled from setWindowSize IMO
    Vector2D windowPos = pWindow->m_vRealPosition.value();

    if (pWindow->m_bIsX11 && PMONITOR) {
        windowPos = windowPos - PMONITOR->vecPosition; // normalize to monitor
        if (*PXWLFORCESCALEZERO)
            windowPos = windowPos * PMONITOR->scale;           // scale if applicable
        windowPos = windowPos + PMONITOR->vecXWaylandPosition; // move to correct position for xwayland
    }

    if (!force && ((pWindow->m_vPendingReportedSize == size && windowPos == pWindow->m_vReportedPosition) || (pWindow->m_vPendingReportedSize == size && !pWindow->m_bIsX11)))
        return;

    pWindow->m_vReportedPosition    = windowPos;
    pWindow->m_vPendingReportedSize = size;

    pWindow->m_fX11SurfaceScaledBy = 1.f;

    if (*PXWLFORCESCALEZERO && pWindow->m_bIsX11 && PMONITOR) {
        size                           = size * PMONITOR->scale;
        pWindow->m_fX11SurfaceScaledBy = PMONITOR->scale;
    }

    if (pWindow->m_bIsX11)
        wlr_xwayland_surface_configure(pWindow->m_uSurface.xwayland, windowPos.x, windowPos.y, size.x, size.y);
    else if (pWindow->m_pXDGSurface->toplevel)
        pWindow->m_vPendingSizeAcks.push_back(std::make_pair<>(pWindow->m_pXDGSurface->toplevel->setSize(size), size.floor()));
}

wlr_surface* CHyprXWaylandManager::surfaceAt(PHLWINDOW pWindow, const Vector2D& client, Vector2D& surface) {
    return wlr_surface_surface_at(pWindow->m_pWLSurface.wlr(), client.x, client.y, &surface.x, &surface.y);
}

bool CHyprXWaylandManager::shouldBeFloated(PHLWINDOW pWindow, bool pending) {
    if (pWindow->m_bIsX11) {
        for (size_t i = 0; i < pWindow->m_uSurface.xwayland->window_type_len; i++)
            if (pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLBAR"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"]) {

                if (pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
                    pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"])
                    pWindow->m_bX11ShouldntFocus = true;

                pWindow->m_bNoInitialFocus = true;
                return true;
            }

        if (pWindow->m_uSurface.xwayland->role) {
            try {
                std::string winrole = std::string(pWindow->m_uSurface.xwayland->role);
                if (winrole.contains("pop-up") || winrole.contains("task_dialog")) {
                    return true;
                }
            } catch (std::exception& e) { Debug::log(ERR, "Error in shouldBeFloated, winrole threw {}", e.what()); }
        }

        if (pWindow->m_uSurface.xwayland->modal) {
            pWindow->m_bIsModal = true;
            return true;
        }

        if (pWindow->m_iX11Type == 2)
            return true; // override_redirect

        const auto SIZEHINTS = pWindow->m_uSurface.xwayland->size_hints;
        if (SIZEHINTS && (pWindow->m_uSurface.xwayland->parent || ((SIZEHINTS->min_width == SIZEHINTS->max_width) && (SIZEHINTS->min_height == SIZEHINTS->max_height))))
            return true;
    } else {
        const auto PSTATE = pending ? &pWindow->m_pXDGSurface->toplevel->pending : &pWindow->m_pXDGSurface->toplevel->current;

        if (pWindow->m_pXDGSurface->toplevel->parent ||
            (PSTATE->minSize.x != 0 && PSTATE->minSize.y != 0 && (PSTATE->minSize.x == PSTATE->maxSize.x || PSTATE->minSize.y == PSTATE->maxSize.y)))
            return true;
    }

    return false;
}

void CHyprXWaylandManager::moveXWaylandWindow(PHLWINDOW pWindow, const Vector2D& pos) {
    if (!validMapped(pWindow))
        return;

    if (!pWindow->m_bIsX11)
        return;

    wlr_xwayland_surface_configure(pWindow->m_uSurface.xwayland, pos.x, pos.y, pWindow->m_vRealSize.value().x, pWindow->m_vRealSize.value().y);
}

void CHyprXWaylandManager::checkBorders(PHLWINDOW pWindow) {
    if (!pWindow->m_bIsX11)
        return;

    for (size_t i = 0; i < pWindow->m_uSurface.xwayland->window_type_len; i++) {
        if (pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_COMBO"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"]) {

            pWindow->m_bX11DoesntWantBorders = true;
            return;
        }
    }

    if (pWindow->m_iX11Type == 2) {
        pWindow->m_bX11DoesntWantBorders = true;
    }
}

void CHyprXWaylandManager::setWindowFullscreen(PHLWINDOW pWindow, bool fullscreen) {
    if (pWindow->m_bIsX11)
        wlr_xwayland_surface_set_fullscreen(pWindow->m_uSurface.xwayland, fullscreen);
    else if (pWindow->m_pXDGSurface && pWindow->m_pXDGSurface->toplevel)
        pWindow->m_pXDGSurface->toplevel->setFullscreen(fullscreen);
}

Vector2D CHyprXWaylandManager::getMaxSizeForWindow(PHLWINDOW pWindow) {
    if (!validMapped(pWindow))
        return Vector2D(99999, 99999);

    if ((pWindow->m_bIsX11 && !pWindow->m_uSurface.xwayland->size_hints) || (!pWindow->m_bIsX11 && !pWindow->m_pXDGSurface->toplevel) || pWindow->m_sAdditionalConfigData.noMaxSize)
        return Vector2D(99999, 99999);

    auto MAXSIZE = pWindow->m_bIsX11 ? Vector2D(pWindow->m_uSurface.xwayland->size_hints->max_width, pWindow->m_uSurface.xwayland->size_hints->max_height) :
                                       pWindow->m_pXDGSurface->toplevel->current.maxSize;

    if (MAXSIZE.x < 5)
        MAXSIZE.x = 99999;
    if (MAXSIZE.y < 5)
        MAXSIZE.y = 99999;

    return MAXSIZE;
}

Vector2D CHyprXWaylandManager::getMinSizeForWindow(PHLWINDOW pWindow) {
    if (!validMapped(pWindow))
        return Vector2D(0, 0);

    if ((pWindow->m_bIsX11 && !pWindow->m_uSurface.xwayland->size_hints) || (!pWindow->m_bIsX11 && !pWindow->m_pXDGSurface->toplevel))
        return Vector2D(0, 0);

    auto MINSIZE = pWindow->m_bIsX11 ? Vector2D(pWindow->m_uSurface.xwayland->size_hints->min_width, pWindow->m_uSurface.xwayland->size_hints->min_height) :
                                       pWindow->m_pXDGSurface->toplevel->current.minSize;

    MINSIZE = MINSIZE.clamp({1, 1});

    return MINSIZE;
}

Vector2D CHyprXWaylandManager::xwaylandToWaylandCoords(const Vector2D& coord) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    CMonitor*   pMonitor     = nullptr;
    double      bestDistance = __FLT_MAX__;
    for (auto& m : g_pCompositor->m_vMonitors) {
        const auto SIZ = *PXWLFORCESCALEZERO ? m->vecTransformedSize : m->vecSize;

        double     distance =
            vecToRectDistanceSquared(coord, {m->vecXWaylandPosition.x, m->vecXWaylandPosition.y}, {m->vecXWaylandPosition.x + SIZ.x - 1, m->vecXWaylandPosition.y + SIZ.y - 1});

        if (distance < bestDistance) {
            bestDistance = distance;
            pMonitor     = m.get();
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->vecXWaylandPosition;
    // if scaled, unscale
    if (*PXWLFORCESCALEZERO)
        result = result / pMonitor->scale;
    // add pos
    result = result + pMonitor->vecPosition;

    return result;
}
