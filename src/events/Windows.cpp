#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/InputManager.hpp"
#include "../render/Renderer.hpp"

// ------------------------------------------------------------ //
//  __          _______ _   _ _____   ______          _______   //
//  \ \        / /_   _| \ | |  __ \ / __ \ \        / / ____|  //
//   \ \  /\  / /  | | |  \| | |  | | |  | \ \  /\  / / (___    //
//    \ \/  \/ /   | | | . ` | |  | | |  | |\ \/  \/ / \___ \   //
//     \  /\  /   _| |_| |\  | |__| | |__| | \  /\  /  ____) |  //
//      \/  \/   |_____|_| \_|_____/ \____/   \/  \/  |_____/   //
//                                                              //
// ------------------------------------------------------------ //

void Events::listener_mapWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_mapWindow);

    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();
    PWINDOW->m_iMonitorID = PMONITOR->ID;
    PWINDOW->m_bMappedX11 = true;
    PWINDOW->m_iWorkspaceID = PMONITOR->activeWorkspace;

    if (g_pXWaylandManager->shouldBeFloated(PWINDOW))
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(PWINDOW);
    else
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);
    
    g_pCompositor->focusWindow(PWINDOW);

    Debug::log(LOG, "Map request dispatched, monitor %s, xywh: %f %f %f %f", PMONITOR->szName.c_str(), PWINDOW->m_vRealPosition.x, PWINDOW->m_vRealPosition.y, PWINDOW->m_vRealSize.x, PWINDOW->m_vRealSize.y);
}

void Events::listener_unmapWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_unmapWindow);

    if (g_pXWaylandManager->getWindowSurface(PWINDOW) == g_pCompositor->m_pLastFocus)
        g_pCompositor->m_pLastFocus = nullptr;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    g_pCompositor->removeWindowFromVectorSafe(PWINDOW);

    Debug::log(LOG, "Window %x unmapped", PWINDOW);
}

void Events::listener_commitWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_commitWindow);

    PWINDOW;
}

void Events::listener_destroyWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_destroyWindow);

    if (g_pXWaylandManager->getWindowSurface(PWINDOW) == g_pCompositor->m_pLastFocus)
        g_pCompositor->m_pLastFocus = nullptr;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    g_pCompositor->removeWindowFromVectorSafe(PWINDOW);

    Debug::log(LOG, "Window %x destroyed", PWINDOW);
}

void Events::listener_setTitleWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_setTitleWindow);

    PWINDOW->m_szTitle = g_pXWaylandManager->getTitle(PWINDOW);

    Debug::log(LOG, "Window %x set title to %s", PWINDOW, PWINDOW->m_szTitle.c_str());
}

void Events::listener_fullscreenWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_fullscreenWindow);

    PWINDOW->m_bIsFullscreen = !PWINDOW->m_bIsFullscreen;

    // todo: do it

    Debug::log(LOG, "Window %x fullscreen to %i", PWINDOW, PWINDOW->m_bIsFullscreen);
}

void Events::listener_activate(wl_listener* listener, void* data) {
    // TODO
}

void Events::listener_activateX11(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_activateX11);

    if (PWINDOW->m_iX11Type == 1 /* Managed */) {
        wlr_xwayland_surface_activate(PWINDOW->m_uSurface.xwayland, 1);
    }
}

void Events::listener_configureX11(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_configureX11);

    const auto E = (wlr_xwayland_surface_configure_event*)data;

    // TODO: ignore if tiled?
    wlr_xwayland_surface_configure(PWINDOW->m_uSurface.xwayland, E->x, E->y, E->width, E->height);
}