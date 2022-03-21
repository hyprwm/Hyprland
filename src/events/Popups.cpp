#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/InputManager.hpp"
#include "../render/Renderer.hpp"

// --------------------------------------------- //
//   _____   ____  _____  _    _ _____   _____   //
//  |  __ \ / __ \|  __ \| |  | |  __ \ / ____|  //
//  | |__) | |  | | |__) | |  | | |__) | (___    //
//  |  ___/| |  | |  ___/| |  | |  ___/ \___ \   //
//  | |    | |__| | |    | |__| | |     ____) |  //
//  |_|     \____/|_|     \____/|_|    |_____/   //
//                                               //
// --------------------------------------------- //

void createNewPopup(wlr_xdg_popup* popup, void* parent, bool parentIsLayer) {

    if (!popup)
        return;

    g_pCompositor->m_lLayerPopups.push_back(SLayerPopup());
    const auto PNEWPOPUP = &g_pCompositor->m_lLayerPopups.back();

    PNEWPOPUP->popup = popup;
    if (parentIsLayer)
        PNEWPOPUP->parentSurface = (SLayerSurface*)parent;
    else
        PNEWPOPUP->parentPopup = (wlr_xdg_popup*)parent;

    wl_signal_add(&popup->base->events.map, &PNEWPOPUP->listen_mapPopup);
    wl_signal_add(&popup->base->events.unmap, &PNEWPOPUP->listen_unmapPopup);
    wl_signal_add(&popup->base->events.destroy, &PNEWPOPUP->listen_destroyPopup);
    wl_signal_add(&popup->base->events.new_popup, &PNEWPOPUP->listen_newPopupFromPopup);
    wl_signal_add(&popup->base->surface->events.commit, &PNEWPOPUP->listen_commitPopup);

    const auto PLAYER = g_pCompositor->getLayerForPopup(PNEWPOPUP);
    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(PLAYER->layerSurface->output);

    wlr_box box = {.x = PMONITOR->vecPosition.x, .y = PMONITOR->vecPosition.y, .width = PMONITOR->vecSize.x, .height = PMONITOR->vecSize.y};

    wlr_xdg_popup_unconstrain_from_box(PNEWPOPUP->popup, &box);
}

void Events::listener_newPopup(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_newPopup);

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    createNewPopup(WLRPOPUP, layersurface, true);

    Debug::log(LOG, "New layer popup created from surface %x", layersurface);
}

void Events::listener_newPopupFromPopup(wl_listener* listener, void* data) {
    SLayerPopup* layerPopup = wl_container_of(listener, layerPopup, listen_newPopupFromPopup);

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    createNewPopup(WLRPOPUP, layerPopup, true);

    Debug::log(LOG, "New layer popup created from popup %x", layerPopup);
}

void Events::listener_destroyPopup(wl_listener* listener, void* data) {
    SLayerPopup* layerPopup = wl_container_of(listener, layerPopup, listen_destroyPopup);

    wl_list_remove(&layerPopup->listen_mapPopup.link);
    wl_list_remove(&layerPopup->listen_unmapPopup.link);
    wl_list_remove(&layerPopup->listen_destroyPopup.link);
    wl_list_remove(&layerPopup->listen_commitPopup.link);
    
    g_pCompositor->m_lLayerPopups.remove(*layerPopup);

    Debug::log(LOG, "Destroyed popup %x", layerPopup);
}

void Events::listener_mapPopup(wl_listener* listener, void* data) {
    SLayerPopup* layerPopup = wl_container_of(listener, layerPopup, listen_mapPopup);

    const auto PLAYER = g_pCompositor->getLayerForPopup(layerPopup);

    wlr_surface_send_enter(layerPopup->popup->base->surface, PLAYER->layerSurface->output);

    Debug::log(LOG, "Mapped popup %x", layerPopup);
}

void Events::listener_unmapPopup(wl_listener* listener, void* data) {
    SLayerPopup* layerPopup = wl_container_of(listener, layerPopup, listen_unmapPopup);

}

void Events::listener_commitPopup(wl_listener* listener, void* data) {
    SLayerPopup* layerPopup = wl_container_of(listener, layerPopup, listen_commitPopup);

}

void createNewPopupXDG(wlr_xdg_popup* popup, void* parent, bool parentIsWindow) {
    if (!popup)
        return;

    g_pCompositor->m_lXDGPopups.push_back(SXDGPopup());
    const auto PNEWPOPUP = &g_pCompositor->m_lXDGPopups.back();

    PNEWPOPUP->popup = popup;
    if (parentIsWindow)
        PNEWPOPUP->parentWindow = (CWindow*)parent;
    else {
        PNEWPOPUP->parentPopup = (wlr_xdg_popup*)parent;
        PNEWPOPUP->parentWindow = g_pCompositor->getWindowForPopup((wlr_xdg_popup*)parent);
    }
        

    wl_signal_add(&popup->base->events.map, &PNEWPOPUP->listen_mapPopupXDG);
    wl_signal_add(&popup->base->events.unmap, &PNEWPOPUP->listen_unmapPopupXDG);
    wl_signal_add(&popup->base->events.destroy, &PNEWPOPUP->listen_destroyPopupXDG);
    wl_signal_add(&popup->base->events.new_popup, &PNEWPOPUP->listen_newPopupFromPopupXDG);

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PNEWPOPUP->parentWindow->m_iMonitorID);

    wlr_box box = {.x = PMONITOR->vecPosition.x, .y = PMONITOR->vecPosition.y, .width = PMONITOR->vecSize.x, .height = PMONITOR->vecSize.y};

    wlr_xdg_popup_unconstrain_from_box(PNEWPOPUP->popup, &box);
}

void Events::listener_newPopupXDG(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_newPopupXDG);

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    createNewPopupXDG(WLRPOPUP, PWINDOW, true);

    Debug::log(LOG, "New layer popup created from XDG window %x -> %s", PWINDOW, PWINDOW->m_szTitle.c_str());
}

void Events::listener_newPopupFromPopupXDG(wl_listener* listener, void* data) {
    SXDGPopup* PPOPUP = wl_container_of(listener, PPOPUP, listen_newPopupFromPopupXDG);

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    createNewPopupXDG(WLRPOPUP, PPOPUP, true);

    Debug::log(LOG, "New layer popup created from XDG popup %x -> %s", PPOPUP, PPOPUP->parentWindow->m_szTitle.c_str());
}

void Events::listener_mapPopupXDG(wl_listener* listener, void* data) {
    
}

void Events::listener_unmapPopupXDG(wl_listener* listener, void* data) {
    
}

void Events::listener_destroyPopupXDG(wl_listener* listener, void* data) {
    SXDGPopup* PPOPUP = wl_container_of(listener, PPOPUP, listen_destroyPopupXDG);

    wl_list_remove(&PPOPUP->listen_mapPopupXDG.link);
    wl_list_remove(&PPOPUP->listen_unmapPopupXDG.link);
    wl_list_remove(&PPOPUP->listen_destroyPopupXDG.link);

    g_pCompositor->m_lXDGPopups.remove(*PPOPUP);

    Debug::log(LOG, "Destroyed popup XDG %x", PPOPUP);
}