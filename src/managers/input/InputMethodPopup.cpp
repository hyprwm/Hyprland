#include "InputMethodPopup.hpp"
#include "InputManager.hpp"
#include "../../Compositor.hpp"

CInputPopup::CInputPopup(wlr_input_popup_surface_v2* surf) : pWlr(surf) {
    surface.assign(surf->surface);
    initCallbacks();
}

static void onCommit(void* owner, void* data) {
    const auto PPOPUP = (CInputPopup*)owner;
    PPOPUP->onCommit();
}

static void onMap(void* owner, void* data) {
    const auto PPOPUP = (CInputPopup*)owner;
    PPOPUP->onMap();
}

static void onUnmap(void* owner, void* data) {
    const auto PPOPUP = (CInputPopup*)owner;
    PPOPUP->onUnmap();
}

static void onDestroy(void* owner, void* data) {
    const auto PPOPUP = (CInputPopup*)owner;
    PPOPUP->onDestroy();
}

void CInputPopup::initCallbacks() {
    hyprListener_commitPopup.initCallback(&pWlr->surface->events.commit, &::onCommit, this, "IME Popup");
    hyprListener_mapPopup.initCallback(&pWlr->surface->events.map, &::onMap, this, "IME Popup");
    hyprListener_unmapPopup.initCallback(&pWlr->surface->events.unmap, &::onUnmap, this, "IME Popup");
    hyprListener_destroyPopup.initCallback(&pWlr->events.destroy, &::onDestroy, this, "IME Popup");
}

CWLSurface* CInputPopup::queryOwner() {
    const auto FOCUSED = g_pInputManager->m_sIMERelay.getFocusedTextInput();

    if (!FOCUSED)
        return nullptr;

    return CWLSurface::surfaceFromWlr(FOCUSED->focusedSurface());
}

void CInputPopup::onDestroy() {
    hyprListener_commitPopup.removeCallback();
    hyprListener_destroyPopup.removeCallback();
    hyprListener_mapPopup.removeCallback();
    hyprListener_unmapPopup.removeCallback();

    g_pInputManager->m_sIMERelay.removePopup(this);
}

void CInputPopup::onMap() {
    Debug::log(LOG, "Mapped an IME Popup");

    updateBox();
    damageEntire();

    const auto PMONITOR = g_pCompositor->getMonitorFromVector(globalBox().middle());

    if (!PMONITOR)
        return;

    g_pProtocolManager->m_pFractionalScaleProtocolManager->setPreferredScaleForSurface(surface.wlr(), PMONITOR->scale);
}

void CInputPopup::onUnmap() {
    Debug::log(LOG, "Unmapped an IME Popup");

    damageEntire();
}

void CInputPopup::onCommit() {
    updateBox();
}

void CInputPopup::damageEntire() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        Debug::log(ERR, "BUG THIS: No owner in imepopup::damageentire");
        return;
    }
    CBox box = globalBox();
    g_pHyprRenderer->damageBox(&box);
}

void CInputPopup::damageSurface() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        Debug::log(ERR, "BUG THIS: No owner in imepopup::damagesurface");
        return;
    }

    Vector2D pos = globalBox().pos();
    g_pHyprRenderer->damageSurface(surface.wlr(), pos.x, pos.y);
}

void CInputPopup::updateBox() {
    if (!surface.wlr()->mapped)
        return;

    const auto OWNER      = queryOwner();
    const auto PFOCUSEDTI = g_pInputManager->m_sIMERelay.getFocusedTextInput();

    if (!PFOCUSEDTI)
        return;

    bool cursorRect      = PFOCUSEDTI->hasCursorRectangle();
    CBox cursorBoxParent = PFOCUSEDTI->cursorBox();

    CBox parentBox;

    if (!OWNER)
        parentBox = {0, 0, 500, 500};
    else
        parentBox = OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 500, 500});

    if (!cursorRect) {
        Vector2D coords = OWNER ? OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 500, 500}).pos() : Vector2D{0, 0};
        parentBox       = {coords, {500, 500}};
        cursorBoxParent = {0, 0, (int)parentBox.w, (int)parentBox.h};
    }

    Vector2D  currentPopupSize = surface.getViewporterCorrectedSize();

    CMonitor* pMonitor = g_pCompositor->getMonitorFromVector(parentBox.middle());

    Vector2D  popupOffset(0, 0);

    if (parentBox.y + cursorBoxParent.y + cursorBoxParent.height + currentPopupSize.y > pMonitor->vecPosition.y + pMonitor->vecSize.y)
        popupOffset.y = -currentPopupSize.y;
    else
        popupOffset.y = cursorBoxParent.height;

    double popupOverflow = parentBox.x + cursorBoxParent.x + currentPopupSize.x - (pMonitor->vecPosition.x + pMonitor->vecSize.x);
    if (popupOverflow > 0)
        popupOffset.x -= popupOverflow;

    CBox cursorBoxLocal({-popupOffset.x, -popupOffset.y}, cursorBoxParent.size());
    wlr_input_popup_surface_v2_send_text_input_rectangle(pWlr, cursorBoxLocal.pWlr());

    CBox popupBoxParent(cursorBoxParent.pos() + popupOffset, currentPopupSize);
    if (popupBoxParent != lastBoxLocal) {
        damageEntire();
        lastBoxLocal = popupBoxParent;
    }
    damageSurface();

    if (const auto PM = g_pCompositor->getMonitorFromCursor(); PM && PM->ID != lastMonitor) {
        const auto PML = g_pCompositor->getMonitorFromID(lastMonitor);

        if (PML)
            wlr_surface_send_leave(surface.wlr(), PML->output);

        wlr_surface_send_enter(surface.wlr(), PM->output);

        lastMonitor = PM->ID;
    }
}

CBox CInputPopup::globalBox() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        Debug::log(ERR, "BUG THIS: No owner in imepopup::globalbox");
        return {};
    }
    CBox parentBox = OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 500, 500});

    return lastBoxLocal.copy().translate(parentBox.pos());
}

bool CInputPopup::isVecInPopup(const Vector2D& point) {
    return globalBox().containsPoint(point);
}

wlr_surface* CInputPopup::getWlrSurface() {
    return surface.wlr();
}
