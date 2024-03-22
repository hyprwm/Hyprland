#include "InputMethodRelay.hpp"
#include "InputManager.hpp"
#include "../../Compositor.hpp"

CInputMethodRelay::CInputMethodRelay() {
    g_pHookSystem->hookDynamic("keyboardFocus", [&](void* self, SCallbackInfo& info, std::any param) { onKeyboardFocus(std::any_cast<wlr_surface*>(param)); });
}

void CInputMethodRelay::onNewIME(wlr_input_method_v2* pIME) {
    if (m_pWLRIME) {
        Debug::log(ERR, "Cannot register 2 IMEs at once!");

        wlr_input_method_v2_send_unavailable(pIME);

        return;
    }

    m_pWLRIME = pIME;

    hyprListener_IMECommit.initCallback(
        &m_pWLRIME->events.commit,
        [&](void* owner, void* data) {
            const auto PTI  = getFocusedTextInput();
            const auto PIMR = (CInputMethodRelay*)owner;

            if (!PTI) {
                Debug::log(LOG, "No focused TextInput on IME Commit");
                return;
            }

            PTI->updateIMEState(PIMR->m_pWLRIME);
        },
        this, "IMERelay");

    hyprListener_IMEDestroy.initCallback(
        &m_pWLRIME->events.destroy,
        [&](void* owner, void* data) {
            m_pWLRIME = nullptr;

            hyprListener_IMEDestroy.removeCallback();
            hyprListener_IMECommit.removeCallback();
            hyprListener_IMEGrab.removeCallback();
            hyprListener_IMENewPopup.removeCallback();

            m_pKeyboardGrab.reset(nullptr);

            const auto PTI = getFocusedTextInput();

            Debug::log(LOG, "IME Destroy");

            if (PTI)
                PTI->enter(PTI->focusedSurface());
        },
        this, "IMERelay");

    hyprListener_IMEGrab.initCallback(
        &m_pWLRIME->events.grab_keyboard,
        [&](void* owner, void* data) {
            Debug::log(LOG, "IME TextInput Keyboard Grab new");

            m_pKeyboardGrab.reset(nullptr);

            m_pKeyboardGrab = std::make_unique<SIMEKbGrab>();

            m_pKeyboardGrab->pKeyboard = wlr_seat_get_keyboard(g_pCompositor->m_sSeat.seat);

            const auto PKBGRAB = (wlr_input_method_keyboard_grab_v2*)data;

            m_pKeyboardGrab->pWlrKbGrab = PKBGRAB;

            wlr_input_method_keyboard_grab_v2_set_keyboard(m_pKeyboardGrab->pWlrKbGrab, m_pKeyboardGrab->pKeyboard);

            m_pKeyboardGrab->hyprListener_grabDestroy.initCallback(
                &PKBGRAB->events.destroy,
                [&](void* owner, void* data) {
                    m_pKeyboardGrab->hyprListener_grabDestroy.removeCallback();

                    Debug::log(LOG, "IME TextInput Keyboard Grab destroy");

                    m_pKeyboardGrab.reset(nullptr);
                },
                m_pKeyboardGrab.get(), "IME Keyboard Grab");
        },
        this, "IMERelay");

    hyprListener_IMENewPopup.initCallback(
        &m_pWLRIME->events.new_popup_surface,
        [&](void* owner, void* data) {
            const auto PNEWPOPUP = &m_lIMEPopups.emplace_back();

            PNEWPOPUP->pSurface = (wlr_input_popup_surface_v2*)data;

            PNEWPOPUP->hyprListener_commitPopup.initCallback(&PNEWPOPUP->pSurface->surface->events.commit, &Events::listener_commitInputPopup, PNEWPOPUP, "IME Popup");
            PNEWPOPUP->hyprListener_mapPopup.initCallback(&PNEWPOPUP->pSurface->surface->events.map, &Events::listener_mapInputPopup, PNEWPOPUP, "IME Popup");
            PNEWPOPUP->hyprListener_unmapPopup.initCallback(&PNEWPOPUP->pSurface->surface->events.unmap, &Events::listener_unmapInputPopup, PNEWPOPUP, "IME Popup");
            PNEWPOPUP->hyprListener_destroyPopup.initCallback(&PNEWPOPUP->pSurface->events.destroy, &Events::listener_destroyInputPopup, PNEWPOPUP, "IME Popup");

            Debug::log(LOG, "New input popup");
        },
        this, "IMERelay");

    if (const auto PTI = getFocusedTextInput(); PTI)
        PTI->enter(PTI->focusedSurface());
}

void CInputMethodRelay::updateInputPopup(SIMEPopup* pPopup) {
    if (!pPopup->pSurface->surface->mapped)
        return;

    // damage last known pos & size
    g_pHyprRenderer->damageBox(&pPopup->lastBox);

    const auto PFOCUSEDTI = getFocusedTextInput();

    if (!PFOCUSEDTI || !PFOCUSEDTI->focusedSurface())
        return;

    bool       cursorRect      = PFOCUSEDTI->hasCursorRectangle();
    const auto PFOCUSEDSURFACE = PFOCUSEDTI->focusedSurface();
    CBox       cursorBox       = PFOCUSEDTI->cursorBox();

    CBox       parentBox;

    const auto PSURFACE = CWLSurface::surfaceFromWlr(PFOCUSEDSURFACE);

    if (!PSURFACE)
        parentBox = {0, 0, 200, 200};
    else
        parentBox = PSURFACE->getSurfaceBoxGlobal().value_or(CBox{0, 0, 200, 200});

    if (!cursorRect)
        cursorBox = {0, 0, (int)parentBox.w, (int)parentBox.h};

    CMonitor* pMonitor = g_pCompositor->getMonitorFromVector(cursorBox.middle());

    if (cursorBox.y + parentBox.y + pPopup->pSurface->surface->current.height + cursorBox.height > pMonitor->vecPosition.y + pMonitor->vecSize.y)
        cursorBox.y -= pPopup->pSurface->surface->current.height + cursorBox.height;

    if (cursorBox.x + parentBox.x + pPopup->pSurface->surface->current.width > pMonitor->vecPosition.x + pMonitor->vecSize.x)
        cursorBox.x -= (cursorBox.x + parentBox.x + pPopup->pSurface->surface->current.width) - (pMonitor->vecPosition.x + pMonitor->vecSize.x);

    pPopup->x = cursorBox.x;
    pPopup->y = cursorBox.y + cursorBox.height;

    pPopup->realX = cursorBox.x + parentBox.x;
    pPopup->realY = cursorBox.y + parentBox.y + cursorBox.height;

    pPopup->lastBox = cursorBox;

    wlr_input_popup_surface_v2_send_text_input_rectangle(pPopup->pSurface, cursorBox.pWlr());

    damagePopup(pPopup);
}

void CInputMethodRelay::setIMEPopupFocus(SIMEPopup* pPopup, wlr_surface* pSurface) {
    updateInputPopup(pPopup);
}

void Events::listener_mapInputPopup(void* owner, void* data) {
    const auto PPOPUP = (SIMEPopup*)owner;

    Debug::log(LOG, "Mapped an IME Popup");

    g_pInputManager->m_sIMERelay.updateInputPopup(PPOPUP);

    if (const auto PMONITOR = g_pCompositor->getMonitorFromVector(PPOPUP->lastBox.middle()); PMONITOR)
        wlr_surface_send_enter(PPOPUP->pSurface->surface, PMONITOR->output);
}

void Events::listener_unmapInputPopup(void* owner, void* data) {
    const auto PPOPUP = (SIMEPopup*)owner;

    Debug::log(LOG, "Unmapped an IME Popup");

    g_pHyprRenderer->damageBox(&PPOPUP->lastBox);

    g_pInputManager->m_sIMERelay.updateInputPopup(PPOPUP);
}

void Events::listener_destroyInputPopup(void* owner, void* data) {
    const auto PPOPUP = (SIMEPopup*)owner;

    Debug::log(LOG, "Removed an IME Popup");

    PPOPUP->hyprListener_commitPopup.removeCallback();
    PPOPUP->hyprListener_destroyPopup.removeCallback();
    PPOPUP->hyprListener_focusedSurfaceUnmap.removeCallback();
    PPOPUP->hyprListener_mapPopup.removeCallback();
    PPOPUP->hyprListener_unmapPopup.removeCallback();

    g_pInputManager->m_sIMERelay.removePopup(PPOPUP);
}

void Events::listener_commitInputPopup(void* owner, void* data) {
    const auto PPOPUP = (SIMEPopup*)owner;

    g_pInputManager->m_sIMERelay.updateInputPopup(PPOPUP);
}

void CInputMethodRelay::removePopup(SIMEPopup* pPopup) {
    m_lIMEPopups.remove(*pPopup);
}

void CInputMethodRelay::damagePopup(SIMEPopup* pPopup) {
    if (!pPopup->pSurface->surface->mapped)
        return;

    const auto PFOCUSEDTI = getFocusedTextInput();

    if (!PFOCUSEDTI || !PFOCUSEDTI->focusedSurface())
        return;

    Vector2D   parentPos;

    const auto PFOCUSEDSURFACE = PFOCUSEDTI->focusedSurface();

    if (wlr_layer_surface_v1_try_from_wlr_surface(PFOCUSEDSURFACE)) {
        const auto PLS = g_pCompositor->getLayerSurfaceFromWlr(wlr_layer_surface_v1_try_from_wlr_surface(PFOCUSEDSURFACE));

        if (PLS) {
            parentPos = Vector2D(PLS->geometry.x, PLS->geometry.y) + g_pCompositor->getMonitorFromID(PLS->monitorID)->vecPosition;
        }
    } else {
        const auto PWINDOW = g_pCompositor->getWindowFromSurface(PFOCUSEDSURFACE);

        if (PWINDOW) {
            parentPos = PWINDOW->m_vRealPosition.goal();
        }
    }

    g_pHyprRenderer->damageSurface(pPopup->pSurface->surface, parentPos.x + pPopup->x, parentPos.y + pPopup->y);
}

SIMEKbGrab* CInputMethodRelay::getIMEKeyboardGrab(SKeyboard* pKeyboard) {

    if (!m_pWLRIME)
        return nullptr;

    if (!m_pKeyboardGrab.get())
        return nullptr;

    const auto VIRTKB = wlr_input_device_get_virtual_keyboard(pKeyboard->keyboard);

    if (VIRTKB && (wl_resource_get_client(VIRTKB->resource) == wl_resource_get_client(m_pKeyboardGrab->pWlrKbGrab->resource)))
        return nullptr;

    return m_pKeyboardGrab.get();
}

CTextInput* CInputMethodRelay::getFocusedTextInput() {
    if (!g_pCompositor->m_pLastFocus)
        return nullptr;

    for (auto& ti : m_vTextInputs) {
        if (ti->focusedSurface() == g_pCompositor->m_pLastFocus)
            return ti.get();
    }

    return nullptr;
}

void CInputMethodRelay::onNewTextInput(wlr_text_input_v3* pInput) {
    m_vTextInputs.emplace_back(std::make_unique<CTextInput>(pInput));
}

void CInputMethodRelay::onNewTextInput(STextInputV1* pTIV1) {
    m_vTextInputs.emplace_back(std::make_unique<CTextInput>(pTIV1));
}

void CInputMethodRelay::removeTextInput(CTextInput* pInput) {
    m_vTextInputs.remove_if([&](const auto& other) { return other.get() == pInput; });
}

void CInputMethodRelay::commitIMEState(CTextInput* pInput) {
    if (!m_pWLRIME)
        return;

    pInput->commitStateToIME(m_pWLRIME);
}

void CInputMethodRelay::onKeyboardFocus(wlr_surface* pSurface) {
    if (!m_pWLRIME)
        return;

    if (pSurface == m_pLastKbFocus)
        return;

    m_pLastKbFocus = pSurface;

    for (auto& ti : m_vTextInputs) {
        if (!ti->focusedSurface())
            continue;

        ti->leave();
    }

    for (auto& ti : m_vTextInputs) {
        if (!ti->isV3())
            continue;

        if (ti->client() != wl_resource_get_client(pSurface->resource))
            continue;

        ti->enter(pSurface);
    }
}
