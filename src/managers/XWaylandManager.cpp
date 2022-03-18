#include "XWaylandManager.hpp"
#include "../Compositor.hpp"
#include "../events/Events.hpp"

CHyprXWaylandManager::CHyprXWaylandManager() {
    m_sWLRXWayland = wlr_xwayland_create(g_pCompositor->m_sWLDisplay, g_pCompositor->m_sWLRCompositor, 1);

    if (!m_sWLRXWayland) {
        Debug::log(ERR, "Couldn't start up the XWaylandManager because wlr_xwayland_create returned a nullptr!");
        return;
    }

    wl_signal_add(&m_sWLRXWayland->events.ready, &Events::listen_readyXWayland);
    wl_signal_add(&m_sWLRXWayland->events.new_surface, &Events::listen_surfaceXWayland);

    setenv("DISPLAY", m_sWLRXWayland->display_name, 1);

    Debug::log(LOG, "CHyprXWaylandManager started on display %s", m_sWLRXWayland->display_name);
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

    else if (wlr_surface_is_xwayland_surface(pSurface))
        wlr_xwayland_surface_activate(wlr_xwayland_surface_from_wlr_surface(pSurface), activate);
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
    if (pWindow->m_bIsX11)
        return pWindow->m_uSurface.xwayland->title;

    return pWindow->m_uSurface.xdg->toplevel->title;
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
        wlr_xwayland_surface_configure(pWindow->m_uSurface.xwayland, pWindow->m_vPosition.x, pWindow->m_vPosition.y, size.x, size.y);

    else 
        wlr_xdg_toplevel_set_size(pWindow->m_uSurface.xdg->toplevel, size.x, size.y);
}

void CHyprXWaylandManager::setWindowStyleTiled(CWindow* pWindow, uint32_t edgez) {
    if (!pWindow->m_bIsX11)
        wlr_xdg_toplevel_set_tiled(pWindow->m_uSurface.xdg->toplevel, edgez);
}

wlr_surface* CHyprXWaylandManager::surfaceAt(CWindow* pWindow, const Vector2D& client, Vector2D& server) {
    if (pWindow->m_bIsX11) 
        return wlr_surface_surface_at(pWindow->m_uSurface.xwayland->surface, client.x, client.y, &server.x, &server.y);

    return wlr_xdg_surface_surface_at(pWindow->m_uSurface.xdg, client.x, client.y, &server.x, &server.y);
}