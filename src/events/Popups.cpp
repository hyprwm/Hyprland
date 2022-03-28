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

void addPopupGlobalCoords(void* pPopup, int* x, int* y) {
    SXDGPopup *const PPOPUP = (SXDGPopup*)pPopup;

    int px = 0;
    int py = 0;

    auto curPopup = PPOPUP;
    while (true) {
        px += curPopup->popup->geometry.x;
        py += curPopup->popup->geometry.y;

        if (curPopup->parentPopup) {
            curPopup = curPopup->parentPopup;
        } else {
            break;
        }
    }

    px += *PPOPUP->lx;
    py += *PPOPUP->ly;

    *x += px;
    *y += py;
}

void createNewPopup(wlr_xdg_popup* popup, SXDGPopup* pHyprPopup) {
    pHyprPopup->popup = popup;

    pHyprPopup->listen_mapPopupXDG.notify = Events::listener_mapPopupXDG;
    pHyprPopup->listen_unmapPopupXDG.notify = Events::listener_unmapPopupXDG;
    pHyprPopup->listen_destroyPopupXDG.notify = Events::listener_destroyPopupXDG;
    pHyprPopup->listen_newPopupFromPopupXDG.notify = Events::listener_newPopupXDG;

    addWLSignal(&popup->base->events.map, &pHyprPopup->listen_mapPopupXDG, pHyprPopup, "HyprPopup");
    addWLSignal(&popup->base->events.unmap, &pHyprPopup->listen_unmapPopupXDG, pHyprPopup, "HyprPopup");
    addWLSignal(&popup->base->surface->events.destroy, &pHyprPopup->listen_destroyPopupXDG, pHyprPopup, "HyprPopup");
    addWLSignal(&popup->base->events.new_popup, &pHyprPopup->listen_newPopupFromPopupXDG, pHyprPopup, "HyprPopup");

    const auto PMONITOR = g_pCompositor->m_pLastMonitor;

    wlr_box box = {.x = PMONITOR->vecPosition.x, .y = PMONITOR->vecPosition.y, .width = PMONITOR->vecSize.x, .height = PMONITOR->vecSize.y};

    wlr_xdg_popup_unconstrain_from_box(popup, &box);
}

void Events::listener_newPopup(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_newPopup);

    Debug::log(LOG, "New layer popup created from surface %x", layersurface);

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    g_pCompositor->m_lXDGPopups.push_back(SXDGPopup());
    const auto PNEWPOPUP = &g_pCompositor->m_lXDGPopups.back();

    PNEWPOPUP->popup = WLRPOPUP;
    PNEWPOPUP->lx = &layersurface->position.x;
    PNEWPOPUP->ly = &layersurface->position.y;
    createNewPopup(WLRPOPUP, PNEWPOPUP);
}

void Events::listener_newPopupXDG(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_newPopupXDG);

    Debug::log(LOG, "New layer popup created from XDG window %x -> %s", PWINDOW, PWINDOW->m_szTitle.c_str());

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    g_pCompositor->m_lXDGPopups.push_back(SXDGPopup());
    const auto PNEWPOPUP = &g_pCompositor->m_lXDGPopups.back();

    PNEWPOPUP->popup = WLRPOPUP;
    PNEWPOPUP->lx = &PWINDOW->m_vEffectivePosition.x;
    PNEWPOPUP->ly = &PWINDOW->m_vEffectivePosition.y;
    createNewPopup(WLRPOPUP, PNEWPOPUP);
}

void Events::listener_newPopupFromPopupXDG(wl_listener* listener, void* data) {
    SXDGPopup* PPOPUP = wl_container_of(listener, PPOPUP, listen_newPopupFromPopupXDG);

    Debug::log(LOG, "New layer popup created from XDG popup %x -> %s", PPOPUP, PPOPUP->parentWindow->m_szTitle.c_str());

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    g_pCompositor->m_lXDGPopups.push_back(SXDGPopup());
    const auto PNEWPOPUP = &g_pCompositor->m_lXDGPopups.back();

    PNEWPOPUP->popup = WLRPOPUP;
    PNEWPOPUP->parentPopup = PPOPUP;
    PNEWPOPUP->lx = PPOPUP->lx;
    PNEWPOPUP->ly = PPOPUP->ly;

    createNewPopup(WLRPOPUP, PNEWPOPUP);
}

void Events::listener_mapPopupXDG(wl_listener* listener, void* data) {
    SXDGPopup* PPOPUP = wl_container_of(listener, PPOPUP, listen_mapPopupXDG);

    Debug::log(LOG, "New XDG Popup mapped");

    PPOPUP->pSurfaceTree = SubsurfaceTree::createTreeRoot(PPOPUP->popup->base->surface, addPopupGlobalCoords, PPOPUP);
}

void Events::listener_unmapPopupXDG(wl_listener* listener, void* data) {
    SXDGPopup* PPOPUP = wl_container_of(listener, PPOPUP, listen_unmapPopupXDG);
    Debug::log(LOG, "XDG Popup unmapped");

    SubsurfaceTree::destroySurfaceTree(PPOPUP->pSurfaceTree);

    PPOPUP->pSurfaceTree = nullptr;
}

void Events::listener_destroyPopupXDG(wl_listener* listener, void* data) {
    SXDGPopup* PPOPUP = wl_container_of(listener, PPOPUP, listen_destroyPopupXDG);

    Debug::log(LOG, "Destroyed popup XDG %x", PPOPUP);

    if (PPOPUP->pSurfaceTree) {
        SubsurfaceTree::destroySurfaceTree(PPOPUP->pSurfaceTree);
        PPOPUP->pSurfaceTree = nullptr;
    }

    g_pCompositor->m_lXDGPopups.remove(*PPOPUP);
}