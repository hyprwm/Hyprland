#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
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
        px += curPopup->popup->current.geometry.x;
        py += curPopup->popup->current.geometry.y;

        // fix oversized fucking popups 
        // kill me
        if (curPopup->pSurfaceTree && curPopup->pSurfaceTree->pSurface && !curPopup->parentPopup) {
            const auto EXTENTSSURFACE = pixman_region32_extents(&curPopup->pSurfaceTree->pSurface->input_region);
            px -= EXTENTSSURFACE->x1;
            py -= EXTENTSSURFACE->y1;
        }

        if (curPopup->parentPopup) {
            curPopup = curPopup->parentPopup;
        } else {
            break;
        }
    }

    px += PPOPUP->lx;
    py += PPOPUP->ly;

    *x += px;
    *y += py;
}

void createNewPopup(wlr_xdg_popup* popup, SXDGPopup* pHyprPopup) {
    pHyprPopup->popup = popup;

    pHyprPopup->hyprListener_destroyPopupXDG.initCallback(&popup->base->events.destroy, &Events::listener_destroyPopupXDG, pHyprPopup, "HyprPopup");
    pHyprPopup->hyprListener_mapPopupXDG.initCallback(&popup->base->events.map, &Events::listener_mapPopupXDG, pHyprPopup, "HyprPopup");
    pHyprPopup->hyprListener_unmapPopupXDG.initCallback(&popup->base->events.unmap, &Events::listener_unmapPopupXDG, pHyprPopup, "HyprPopup");
    pHyprPopup->hyprListener_newPopupFromPopupXDG.initCallback(&popup->base->events.new_popup, &Events::listener_newPopupFromPopupXDG, pHyprPopup, "HyprPopup");
    pHyprPopup->hyprListener_commitPopupXDG.initCallback(&popup->base->surface->events.commit, &Events::listener_commitPopupXDG, pHyprPopup, "HyprPopup");

    const auto PMONITOR = g_pCompositor->m_pLastMonitor;

    wlr_box box = {.x = PMONITOR->vecPosition.x - pHyprPopup->lx, .y = PMONITOR->vecPosition.y - pHyprPopup->ly, .width = PMONITOR->vecSize.x, .height = PMONITOR->vecSize.y};

    wlr_xdg_popup_unconstrain_from_box(popup, &box);

    pHyprPopup->monitor = PMONITOR;

    Debug::log(LOG, "Popup: Unconstrained from lx ly: %f %f, pHyprPopup lx ly: %f %f", (float)PMONITOR->vecPosition.x, (float)PMONITOR->vecPosition.y, (float)pHyprPopup->lx, (float)pHyprPopup->ly);
}

void Events::listener_newPopup(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    ASSERT(layersurface);

    Debug::log(LOG, "New layer popup created from surface %x", layersurface);

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    const auto PNEWPOPUP = g_pCompositor->m_vXDGPopups.emplace_back(std::make_unique<SXDGPopup>()).get();

    const auto PMONITOR = g_pCompositor->getMonitorFromID(layersurface->monitorID);

    PNEWPOPUP->popup = WLRPOPUP;
    PNEWPOPUP->lx = layersurface->position.x;
    PNEWPOPUP->ly = layersurface->position.y;
    PNEWPOPUP->monitor = PMONITOR;
    createNewPopup(WLRPOPUP, PNEWPOPUP);
}

void Events::listener_newPopupXDG(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    ASSERT(PWINDOW);

    if (!PWINDOW->m_bIsMapped)
        return;

    Debug::log(LOG, "New layer popup created from XDG window %x -> %s", PWINDOW, PWINDOW->m_szTitle.c_str());

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    const auto PNEWPOPUP = g_pCompositor->m_vXDGPopups.emplace_back(std::make_unique<SXDGPopup>()).get();

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    wlr_box geom;
    wlr_xdg_surface_get_geometry(PWINDOW->m_uSurface.xdg, &geom);

    PNEWPOPUP->popup = WLRPOPUP;
    PNEWPOPUP->lx = PWINDOW->m_vRealPosition.goalv().x - geom.x;
    PNEWPOPUP->ly = PWINDOW->m_vRealPosition.goalv().y - geom.y;
    PNEWPOPUP->parentWindow = PWINDOW;
    PNEWPOPUP->monitor = PMONITOR;
    createNewPopup(WLRPOPUP, PNEWPOPUP);
}

void Events::listener_newPopupFromPopupXDG(void* owner, void* data) {
    SXDGPopup* PPOPUP = (SXDGPopup*)owner;

    ASSERT(PPOPUP);
    
    if (PPOPUP->parentWindow)
        Debug::log(LOG, "New popup created from XDG Window popup %x -> %s", PPOPUP, PPOPUP->parentWindow->m_szTitle.c_str());
    else
        Debug::log(LOG, "New popup created from Non-Window popup %x", PPOPUP);

    const auto WLRPOPUP = (wlr_xdg_popup*)data;

    const auto PNEWPOPUP = g_pCompositor->m_vXDGPopups.emplace_back(std::make_unique<SXDGPopup>()).get();

    PNEWPOPUP->popup = WLRPOPUP;
    PNEWPOPUP->parentPopup = PPOPUP;
    PNEWPOPUP->lx = PPOPUP->lx;
    PNEWPOPUP->ly = PPOPUP->ly;
    PNEWPOPUP->parentWindow = PPOPUP->parentWindow;
    PNEWPOPUP->monitor = PPOPUP->monitor;

    createNewPopup(WLRPOPUP, PNEWPOPUP);
}

void Events::listener_mapPopupXDG(void* owner, void* data) {
    SXDGPopup* PPOPUP = (SXDGPopup*)owner;

    ASSERT(PPOPUP);

    Debug::log(LOG, "New XDG Popup mapped at %d %d", (int)PPOPUP->lx, (int)PPOPUP->ly);

    PPOPUP->pSurfaceTree = SubsurfaceTree::createTreeRoot(PPOPUP->popup->base->surface, addPopupGlobalCoords, PPOPUP, PPOPUP->parentWindow);

    int lx = 0, ly = 0;
    addPopupGlobalCoords(PPOPUP, &lx, &ly);

    g_pHyprRenderer->damageBox(lx, ly, PPOPUP->popup->current.geometry.width, PPOPUP->popup->current.geometry.width);

    Debug::log(LOG, "XDG Popup got assigned a surfaceTreeNode %x", PPOPUP->pSurfaceTree);
}

void Events::listener_unmapPopupXDG(void* owner, void* data) {
    SXDGPopup* PPOPUP = (SXDGPopup*)owner;
    Debug::log(LOG, "XDG Popup unmapped");

    ASSERT(PPOPUP);

    SubsurfaceTree::destroySurfaceTree(PPOPUP->pSurfaceTree);

    int lx = 0, ly = 0;
    addPopupGlobalCoords(PPOPUP, &lx, &ly);

    g_pHyprRenderer->damageBox(lx, ly, PPOPUP->popup->current.geometry.width, PPOPUP->popup->current.geometry.width);

    PPOPUP->pSurfaceTree = nullptr;
}

void Events::listener_commitPopupXDG(void* owner, void* data) {
    SXDGPopup* PPOPUP = (SXDGPopup*)owner;

    int lx = 0, ly = 0;
    addPopupGlobalCoords(PPOPUP, &lx, &ly);

    g_pHyprRenderer->damageSurface(PPOPUP->popup->base->surface, lx, ly);
}

void Events::listener_destroyPopupXDG(void* owner, void* data) {
    SXDGPopup* PPOPUP = (SXDGPopup*)owner;

    ASSERT(PPOPUP);

    Debug::log(LOG, "Destroyed popup XDG %x", PPOPUP);

    if (PPOPUP->pSurfaceTree) {
        SubsurfaceTree::destroySurfaceTree(PPOPUP->pSurfaceTree);
        PPOPUP->pSurfaceTree = nullptr;
    }

    g_pCompositor->m_vXDGPopups.erase(std::remove_if(g_pCompositor->m_vXDGPopups.begin(), g_pCompositor->m_vXDGPopups.end(), [&](std::unique_ptr<SXDGPopup>& el) { return el.get() == PPOPUP; }));
}