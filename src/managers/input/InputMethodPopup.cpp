#include "InputMethodPopup.hpp"
#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/FractionalScale.hpp"
#include "../../protocols/InputMethodV2.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../render/Renderer.hpp"

CInputPopup::CInputPopup(SP<CInputMethodPopupV2> popup_) : popup(popup_) {
    m_listeners.commit  = popup_->events.commit.registerListener([this](std::any d) { onCommit(); });
    m_listeners.map     = popup_->events.map.registerListener([this](std::any d) { onMap(); });
    m_listeners.unmap   = popup_->events.unmap.registerListener([this](std::any d) { onUnmap(); });
    m_listeners.destroy = popup_->events.destroy.registerListener([this](std::any d) { onDestroy(); });
    surface             = CWLSurface::create();
    surface->assign(popup_->surface());
}

SP<CWLSurface> CInputPopup::queryOwner() {
    const auto FOCUSED = g_pInputManager->m_sIMERelay.getFocusedTextInput();

    if (!FOCUSED)
        return nullptr;

    return CWLSurface::fromResource(FOCUSED->focusedSurface());
}

void CInputPopup::onDestroy() {
    g_pInputManager->m_sIMERelay.removePopup(this);
}

void CInputPopup::onMap() {
    NDebug::log(LOG, "Mapped an IME Popup");

    updateBox();
    damageEntire();

    const auto PMONITOR = g_pCompositor->getMonitorFromVector(globalBox().middle());

    if (!PMONITOR)
        return;

    PROTO::fractional->sendScale(surface->resource(), PMONITOR->scale);
}

void CInputPopup::onUnmap() {
    NDebug::log(LOG, "Unmapped an IME Popup");

    damageEntire();
}

void CInputPopup::onCommit() {
    updateBox();
}

void CInputPopup::damageEntire() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        NDebug::log(ERR, "BUG THIS: No owner in imepopup::damageentire");
        return;
    }
    CBox box = globalBox();
    g_pHyprRenderer->damageBox(box);
}

void CInputPopup::damageSurface() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        NDebug::log(ERR, "BUG THIS: No owner in imepopup::damagesurface");
        return;
    }

    Vector2D pos = globalBox().pos();
    g_pHyprRenderer->damageSurface(surface->resource(), pos.x, pos.y);
}

void CInputPopup::updateBox() {
    if (!popup->mapped)
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

    Vector2D   currentPopupSize = surface->getViewporterCorrectedSize() / surface->resource()->current.scale;

    PHLMONITOR pMonitor = g_pCompositor->getMonitorFromVector(parentBox.middle());

    Vector2D   popupOffset(0, 0);

    if (parentBox.y + cursorBoxParent.y + cursorBoxParent.height + currentPopupSize.y > pMonitor->vecPosition.y + pMonitor->vecSize.y)
        popupOffset.y -= currentPopupSize.y;
    else
        popupOffset.y = cursorBoxParent.height;

    double popupOverflow = parentBox.x + cursorBoxParent.x + currentPopupSize.x - (pMonitor->vecPosition.x + pMonitor->vecSize.x);
    if (popupOverflow > 0)
        popupOffset.x -= popupOverflow;

    CBox cursorBoxLocal({-popupOffset.x, -popupOffset.y}, cursorBoxParent.size());
    popup->sendInputRectangle(cursorBoxLocal);

    CBox popupBoxParent(cursorBoxParent.pos() + popupOffset, currentPopupSize);
    if (popupBoxParent != lastBoxLocal) {
        damageEntire();
        lastBoxLocal = popupBoxParent;
    }
    damageSurface();

    if (const auto PM = g_pCompositor->getMonitorFromCursor(); PM && PM->ID != lastMonitor) {
        const auto PML = g_pCompositor->getMonitorFromID(lastMonitor);

        if (PML)
            surface->resource()->leave(PML->self.lock());

        surface->resource()->enter(PM->self.lock());

        lastMonitor = PM->ID;
    }
}

CBox CInputPopup::globalBox() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        NDebug::log(ERR, "BUG THIS: No owner in imepopup::globalbox");
        return {};
    }
    CBox parentBox = OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 500, 500});

    return lastBoxLocal.copy().translate(parentBox.pos());
}

bool CInputPopup::isVecInPopup(const Vector2D& point) {
    return globalBox().containsPoint(point);
}

SP<CWLSurfaceResource> CInputPopup::getSurface() {
    return surface->resource();
}
