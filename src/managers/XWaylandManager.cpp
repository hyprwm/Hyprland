#include "XWaylandManager.hpp"
#include "../Compositor.hpp"
#include "../events/Events.hpp"

CHyprXWaylandManager::CHyprXWaylandManager() {
    if (XWAYLAND) {
        m_sWLRXWayland = wlr_xwayland_create(g_pCompositor->m_sWLDisplay, g_pCompositor->m_sWLRCompositor, 1);

        if (!m_sWLRXWayland) {
            Debug::log(ERR, "Couldn't start up the XWaylandManager because wlr_xwayland_create returned a nullptr!");
            return;
        }

        addWLSignal(&m_sWLRXWayland->events.ready, &Events::listen_readyXWayland, m_sWLRXWayland, "XWayland Manager");
        addWLSignal(&m_sWLRXWayland->events.new_surface, &Events::listen_surfaceXWayland, m_sWLRXWayland, "XWayland Manager");

        setenv("DISPLAY", m_sWLRXWayland->display_name, 1);

        Debug::log(LOG, "CHyprXWaylandManager started on display %s", m_sWLRXWayland->display_name);
    }
}

CHyprXWaylandManager::~CHyprXWaylandManager() {

}

wlr_surface* CHyprXWaylandManager::getWindowSurface(CWindow* pWindow) {
    if (pWindow->m_bIsX11)
        return pWindow->m_uSurface.xwayland->surface;

    return pWindow->m_uSurface.xdg->surface;
}

void CHyprXWaylandManager::activateSurface(wlr_surface* pSurface, bool activate) {
    if (wlr_surface_is_xdg_surface(pSurface))
        wlr_xdg_toplevel_set_activated(wlr_xdg_surface_from_wlr_surface(pSurface)->toplevel, activate);
    else if (wlr_surface_is_xwayland_surface(pSurface)) {
        wlr_xwayland_surface_activate(wlr_xwayland_surface_from_wlr_surface(pSurface), activate);

        if (activate)
            wlr_xwayland_surface_restack(wlr_xwayland_surface_from_wlr_surface(pSurface), NULL, XCB_STACK_MODE_ABOVE);
    }
        
}

void CHyprXWaylandManager::activateWindow(CWindow* pWindow, bool activate) {
    if (pWindow->m_bIsX11) {
        if (pWindow->m_uSurface.xwayland->minimized)
            wlr_xwayland_surface_set_minimized(pWindow->m_uSurface.xwayland, false);

        wlr_xwayland_surface_activate(pWindow->m_uSurface.xwayland, activate);
        wlr_xwayland_surface_restack(pWindow->m_uSurface.xwayland, NULL, XCB_STACK_MODE_ABOVE);
    }
    else
        wlr_xdg_toplevel_set_activated(pWindow->m_uSurface.xdg->toplevel, activate);

    g_pCompositor->m_pLastFocus = getWindowSurface(pWindow);
    g_pCompositor->m_pLastWindow = pWindow;
}

void CHyprXWaylandManager::getGeometryForWindow(CWindow* pWindow, wlr_box* pbox) {
    if (pWindow->m_bIsX11) {
        pbox->x = pWindow->m_uSurface.xwayland->x;
        pbox->y = pWindow->m_uSurface.xwayland->y;
        pbox->width = pWindow->m_uSurface.xwayland->width;
        pbox->height = pWindow->m_uSurface.xwayland->height;
    } else {
        wlr_xdg_surface_get_geometry(pWindow->m_uSurface.xdg, pbox);
    }
}

std::string CHyprXWaylandManager::getTitle(CWindow* pWindow) {
    try {
        if (pWindow->m_bIsX11) {
            if (pWindow->m_uSurface.xwayland && pWindow->m_uSurface.xwayland->title) {
                return std::string(pWindow->m_uSurface.xwayland->title);
            }
        } else if (pWindow->m_uSurface.xdg) {
            if (pWindow->m_uSurface.xdg->toplevel && pWindow->m_uSurface.xdg->toplevel->title) {
                return std::string(pWindow->m_uSurface.xdg->toplevel->title);
            }
        } else {
            return "";
        }
    } catch (...) {
        Debug::log(ERR, "Error in getTitle (probably null title)");
    }

    return "";
}

std::string CHyprXWaylandManager::getAppIDClass(CWindow* pWindow) {
    try {
        if (pWindow->m_bIsX11) {
            if (pWindow->m_uSurface.xwayland) {
                if (!pWindow->m_bMappedX11 || !pWindow->m_bIsMapped)
                    return "unmanaged X11";

                return std::string(pWindow->m_uSurface.xwayland->_class);
            }
        } else if (pWindow->m_uSurface.xdg) {
            if (pWindow->m_uSurface.xdg->toplevel) {
                return std::string(pWindow->m_uSurface.xdg->toplevel->app_id);
            }
        } else {
            return "";
        }
    } catch (std::logic_error& e) {
        Debug::log(ERR, "Error in getAppIDClass: %s", e.what());
    }

    return "";
}

void CHyprXWaylandManager::sendCloseWindow(CWindow* pWindow) {
    if (pWindow->m_bIsX11) {
        wlr_xwayland_surface_close(pWindow->m_uSurface.xwayland);
    } else {
        wlr_xdg_toplevel_send_close(pWindow->m_uSurface.xdg->toplevel);
    }
}

void CHyprXWaylandManager::setWindowSize(CWindow* pWindow, const Vector2D& size) {
    if (pWindow->m_bIsX11)
        wlr_xwayland_surface_configure(pWindow->m_uSurface.xwayland, pWindow->m_vRealPosition.vec().x, pWindow->m_vRealPosition.vec().y, size.x, size.y);
    else
        wlr_xdg_toplevel_set_size(pWindow->m_uSurface.xdg->toplevel, size.x, size.y);
}

void CHyprXWaylandManager::setWindowStyleTiled(CWindow* pWindow, uint32_t edgez) {
    if (!pWindow->m_bIsX11)
        wlr_xdg_toplevel_set_tiled(pWindow->m_uSurface.xdg->toplevel, edgez);
}

wlr_surface* CHyprXWaylandManager::surfaceAt(CWindow* pWindow, const Vector2D& client, Vector2D& surface) {
    if (pWindow->m_bIsX11)
        return wlr_surface_surface_at(pWindow->m_uSurface.xwayland->surface, client.x, client.y, &surface.x, &surface.y);

    return wlr_xdg_surface_surface_at(pWindow->m_uSurface.xdg, client.x, client.y, &surface.x, &surface.y);
}

bool CHyprXWaylandManager::shouldBeFloated(CWindow* pWindow) {
    if (pWindow->m_bIsX11) {
        for (size_t i = 0; i < pWindow->m_uSurface.xwayland->window_type_len; i++)
            if (pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"] || pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLBAR"] || pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"] || pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"] || pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"])
                    {
                pWindow->m_bNoInitialFocus = true;
                return true;
                    }

        if (pWindow->m_uSurface.xwayland->role) {
            try {
                std::string winrole = std::string(pWindow->m_uSurface.xwayland->role);
                if (winrole.contains("pop-up") || winrole.contains("task_dialog")) {
                    return true;
                }
            } catch (std::exception& e) {
                Debug::log(ERR, "Error in shouldBeFloated, winrole threw %s", e.what());
            }
        }

        if (pWindow->m_uSurface.xwayland->modal) {
            pWindow->m_bIsModal = true;
            return true;
        }

        if (pWindow->m_iX11Type == 2) {
            return true; // override_redirect
        }

        const auto SIZEHINTS = pWindow->m_uSurface.xwayland->size_hints;
        if (SIZEHINTS && (pWindow->m_uSurface.xwayland->parent || ((SIZEHINTS->min_width == SIZEHINTS->max_width) && (SIZEHINTS->min_height == SIZEHINTS->max_height))))
            return true;
    } else {
        const auto PSTATE = &pWindow->m_uSurface.xdg->toplevel->current;
        
        if ((PSTATE->min_width != 0 && PSTATE->min_height != 0 && (PSTATE->min_width == PSTATE->max_width || PSTATE->min_height == PSTATE->max_height)) || pWindow->m_uSurface.xdg->toplevel->parent)
            return true;
    }

    return false;
}

void CHyprXWaylandManager::moveXWaylandWindow(CWindow* pWindow, const Vector2D& pos) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;
        
    if (pWindow->m_bIsX11) {
        wlr_xwayland_surface_configure(pWindow->m_uSurface.xwayland, pos.x, pos.y, pWindow->m_vRealSize.vec().x, pWindow->m_vRealSize.vec().y);
    }
}

void CHyprXWaylandManager::checkBorders(CWindow* pWindow) {
    if (!pWindow->m_bIsX11)
        return;

    for (size_t i = 0; i < pWindow->m_uSurface.xwayland->window_type_len; i++) {
        if (pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] || pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] || pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_COMBO"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] || pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"]) {
            
            pWindow->m_bX11DoesntWantBorders = true;
            return;
        }
    }

    if (pWindow->m_uSurface.xwayland->parent || pWindow->m_iX11Type == 2) {
        pWindow->m_bX11DoesntWantBorders = true;
    }
}

void CHyprXWaylandManager::setWindowFullscreen(CWindow* pWindow, bool fullscreen) {
    if (pWindow->m_bIsX11) {
        wlr_xwayland_surface_set_fullscreen(pWindow->m_uSurface.xwayland, fullscreen);
    } else {
        wlr_xdg_toplevel_set_fullscreen(pWindow->m_uSurface.xdg->toplevel, fullscreen);
    }

    if (pWindow->m_phForeignToplevel)
        wlr_foreign_toplevel_handle_v1_set_fullscreen(pWindow->m_phForeignToplevel, fullscreen);
}