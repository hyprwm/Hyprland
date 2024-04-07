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
                PTI->leave();
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
            m_vIMEPopups.emplace_back(std::make_unique<CInputPopup>((wlr_input_popup_surface_v2*)data));

            Debug::log(LOG, "New input popup");
        },
        this, "IMERelay");

    if (!g_pCompositor->m_pLastFocus)
        return;

    for (auto& ti : m_vTextInputs) {
        if (ti->client() != wl_resource_get_client(g_pCompositor->m_pLastFocus->resource))
            continue;

        if (ti->isV3())
            ti->enter(g_pCompositor->m_pLastFocus);
        else
            ti->onEnabled(g_pCompositor->m_pLastFocus);
    }
}

void CInputMethodRelay::setIMEPopupFocus(CInputPopup* pPopup, wlr_surface* pSurface) {
    pPopup->onCommit();
}

void CInputMethodRelay::removePopup(CInputPopup* pPopup) {
    std::erase_if(m_vIMEPopups, [pPopup](const auto& other) { return other.get() == pPopup; });
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
    std::erase_if(m_vTextInputs, [pInput](const auto& other) { return other.get() == pInput; });
}

void CInputMethodRelay::updateAllPopups() {
    for (auto& p : m_vIMEPopups) {
        p->onCommit();
    }
}

void CInputMethodRelay::activateIME(CTextInput* pInput) {
    if (!m_pWLRIME)
        return;

    wlr_input_method_v2_send_activate(g_pInputManager->m_sIMERelay.m_pWLRIME);
    commitIMEState(pInput);
}

void CInputMethodRelay::deactivateIME(CTextInput* pInput) {
    if (!m_pWLRIME)
        return;

    if (!m_pWLRIME->active)
        return;

    wlr_input_method_v2_send_deactivate(g_pInputManager->m_sIMERelay.m_pWLRIME);
    commitIMEState(pInput);
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

CInputPopup* CInputMethodRelay::popupFromCoords(const Vector2D& point) {
    for (auto& p : m_vIMEPopups) {
        if (p->isVecInPopup(point))
            return p.get();
    }

    return nullptr;
}

CInputPopup* CInputMethodRelay::popupFromSurface(const wlr_surface* surface) {
    for (auto& p : m_vIMEPopups) {
        if (p->getWlrSurface() == surface)
            return p.get();
    }

    return nullptr;
}
