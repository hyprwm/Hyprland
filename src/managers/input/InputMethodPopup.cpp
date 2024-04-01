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

    Vector2D pos = OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 0, 0}).pos() + lastBoxLocal.pos();
    g_pHyprRenderer->damageBox(pos.x, pos.y, surface.wlr()->current.width, surface.wlr()->current.height);
}

void CInputPopup::damageSurface() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        Debug::log(ERR, "BUG THIS: No owner in imepopup::damagesurface");
        return;
    }

    Vector2D pos = OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 0, 0}).pos() + lastBoxLocal.pos();
    g_pHyprRenderer->damageSurface(surface.wlr(), pos.x, pos.y);
}

void CInputPopup::updateBox() {
    if (!surface.wlr()->mapped)
        return;

    const auto OWNER      = queryOwner();
    const auto PFOCUSEDTI = g_pInputManager->m_sIMERelay.getFocusedTextInput();

    if (!PFOCUSEDTI)
        return;

    bool cursorRect     = PFOCUSEDTI->hasCursorRectangle();
    CBox cursorBoxLocal = PFOCUSEDTI->cursorBox();

    CBox parentBox;

    if (!OWNER)
        parentBox = {0, 0, 500, 500};
    else
        parentBox = OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 500, 500});

    if (!cursorRect) {
        Vector2D coords = OWNER ? OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 500, 500}).pos() : Vector2D{0, 0};
        parentBox       = {coords, {500, 500}};
        cursorBoxLocal  = {0, 0, (int)parentBox.w, (int)parentBox.h};
    }

    if (cursorBoxLocal != lastBoxLocal)
        damageEntire();

    CMonitor* pMonitor = g_pCompositor->getMonitorFromVector(parentBox.middle());

    if (cursorBoxLocal.y + parentBox.y + surface.wlr()->current.height + cursorBoxLocal.height > pMonitor->vecPosition.y + pMonitor->vecSize.y)
        cursorBoxLocal.y -= surface.wlr()->current.height;
    else
        cursorBoxLocal.y += cursorBoxLocal.height;

    if (cursorBoxLocal.x + parentBox.x + surface.wlr()->current.width > pMonitor->vecPosition.x + pMonitor->vecSize.x)
        cursorBoxLocal.x -= (cursorBoxLocal.x + parentBox.x + surface.wlr()->current.width) - (pMonitor->vecPosition.x + pMonitor->vecSize.x);

    lastBoxLocal = cursorBoxLocal;

    wlr_input_popup_surface_v2_send_text_input_rectangle(pWlr, cursorBoxLocal.pWlr());

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

    return lastBoxLocal.copy().translate(OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 0, 0}).pos());
}

bool CInputPopup::isVecInPopup(const Vector2D& point) {
    return globalBox().containsPoint(point);
}

wlr_surface* CInputPopup::getWlrSurface() {
    return surface.wlr();
}
