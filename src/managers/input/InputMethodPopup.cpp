#include "InputMethodPopup.hpp"
#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/FractionalScale.hpp"
#include "../../protocols/InputMethodV2.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../render/Renderer.hpp"

CInputPopup::CInputPopup(SP<CInputMethodPopupV2> popup_) : m_popup(popup_) {
    m_listeners.commit  = popup_->m_events.commit.listen([this] { onCommit(); });
    m_listeners.map     = popup_->m_events.map.listen([this] { onMap(); });
    m_listeners.unmap   = popup_->m_events.unmap.listen([this] { onUnmap(); });
    m_listeners.destroy = popup_->m_events.destroy.listen([this] { onDestroy(); });
    m_surface           = CWLSurface::create();
    m_surface->assign(popup_->surface());
}

SP<CWLSurface> CInputPopup::queryOwner() {
    const auto FOCUSED = g_pInputManager->m_relay.getFocusedTextInput();

    if (!FOCUSED)
        return nullptr;

    return CWLSurface::fromResource(FOCUSED->focusedSurface());
}

void CInputPopup::onDestroy() {
    g_pInputManager->m_relay.removePopup(this);
}

void CInputPopup::onMap() {
    Debug::log(LOG, "Mapped an IME Popup");

    updateBox();
    damageEntire();

    const auto PMONITOR = g_pCompositor->getMonitorFromVector(globalBox().middle());

    if (!PMONITOR)
        return;

    PROTO::fractional->sendScale(m_surface->resource(), PMONITOR->m_scale);
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
    g_pHyprRenderer->damageBox(box);
}

void CInputPopup::damageSurface() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        Debug::log(ERR, "BUG THIS: No owner in imepopup::damagesurface");
        return;
    }

    Vector2D pos = globalBox().pos();
    g_pHyprRenderer->damageSurface(m_surface->resource(), pos.x, pos.y);
}

void CInputPopup::updateBox() {
    if (!m_popup->m_mapped)
        return;

    const auto OWNER      = queryOwner();
    const auto PFOCUSEDTI = g_pInputManager->m_relay.getFocusedTextInput();

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
        cursorBoxParent = {0, 0, static_cast<int>(parentBox.w), static_cast<int>(parentBox.h)};
    }

    Vector2D   currentPopupSize = m_surface->getViewporterCorrectedSize() / m_surface->resource()->m_current.scale;

    PHLMONITOR pMonitor = g_pCompositor->getMonitorFromVector(parentBox.middle());

    Vector2D   popupOffset(0, 0);

    if (parentBox.y + cursorBoxParent.y + cursorBoxParent.height + currentPopupSize.y > pMonitor->m_position.y + pMonitor->m_size.y)
        popupOffset.y -= currentPopupSize.y;
    else
        popupOffset.y = cursorBoxParent.height;

    double popupOverflow = parentBox.x + cursorBoxParent.x + currentPopupSize.x - (pMonitor->m_position.x + pMonitor->m_size.x);
    if (popupOverflow > 0)
        popupOffset.x -= popupOverflow;

    CBox cursorBoxLocal({-popupOffset.x, -popupOffset.y}, cursorBoxParent.size());
    m_popup->sendInputRectangle(cursorBoxLocal);

    CBox popupBoxParent(cursorBoxParent.pos() + popupOffset, currentPopupSize);
    if (popupBoxParent != m_lastBoxLocal) {
        damageEntire();
        m_lastBoxLocal = popupBoxParent;
    }
    damageSurface();

    if (const auto PM = g_pCompositor->getMonitorFromCursor(); PM && PM->m_id != m_lastMonitor) {
        const auto PML = g_pCompositor->getMonitorFromID(m_lastMonitor);

        if (PML)
            m_surface->resource()->leave(PML->m_self.lock());

        m_surface->resource()->enter(PM->m_self.lock());

        m_lastMonitor = PM->m_id;
    }
}

CBox CInputPopup::globalBox() {
    const auto OWNER = queryOwner();

    if (!OWNER) {
        Debug::log(ERR, "BUG THIS: No owner in imepopup::globalbox");
        return {};
    }
    CBox parentBox = OWNER->getSurfaceBoxGlobal().value_or(CBox{0, 0, 500, 500});

    return m_lastBoxLocal.copy().translate(parentBox.pos());
}

bool CInputPopup::isVecInPopup(const Vector2D& point) {
    return globalBox().containsPoint(point);
}

SP<CWLSurfaceResource> CInputPopup::getSurface() {
    return m_surface->resource();
}
