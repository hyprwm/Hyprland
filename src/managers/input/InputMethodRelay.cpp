#include "InputMethodRelay.hpp"
#include "InputManager.hpp"
#include "../../Compositor.hpp"

CInputMethodRelay::CInputMethodRelay() {
    
}

void CInputMethodRelay::onNewIME(wlr_input_method_v2* pIME) {
    if (m_pWLRIME) {
        Debug::log(ERR, "Cannot register 2 IMEs at once!");

        wlr_input_method_v2_send_unavailable(pIME);

        return;
    }

    m_pWLRIME = pIME;

    hyprListener_IMECommit.initCallback(&m_pWLRIME->events.commit, [&](void* owner, void* data) {

        const auto PTI = getFocusedTextInput();
        const auto PIMR = (CInputMethodRelay*)owner;

        if (!PTI) {
            Debug::log(LOG, "No focused TextInput on IME Commit");
            return;
        }

        Debug::log(LOG, "IME Commit");

        if (PIMR->m_pWLRIME->current.preedit.text) {
            Debug::log(LOG, "IME TextInput preedit");
            wlr_text_input_v3_send_preedit_string(PTI->pWlrInput, PIMR->m_pWLRIME->current.preedit.text, PIMR->m_pWLRIME->current.preedit.cursor_begin, PIMR->m_pWLRIME->current.preedit.cursor_end);
        }

        if (PIMR->m_pWLRIME->current.commit_text) {
            Debug::log(LOG, "IME TextInput commit");
            wlr_text_input_v3_send_commit_string(PTI->pWlrInput, PIMR->m_pWLRIME->current.commit_text);
        }

        if (PIMR->m_pWLRIME->current.delete_.before_length || PIMR->m_pWLRIME->current.delete_.after_length) {
            Debug::log(LOG, "IME TextInput delete");
            wlr_text_input_v3_send_delete_surrounding_text(PTI->pWlrInput, PIMR->m_pWLRIME->current.delete_.before_length, PIMR->m_pWLRIME->current.delete_.after_length);
        }

        wlr_text_input_v3_send_done(PTI->pWlrInput);

    }, this, "IMERelay");

    hyprListener_IMEDestroy.initCallback(&m_pWLRIME->events.destroy, [&](void* owner, void* data) {

        m_pWLRIME = nullptr;

        hyprListener_IMEDestroy.removeCallback();
        hyprListener_IMECommit.removeCallback();
        hyprListener_IMEGrab.removeCallback();

        m_pKeyboardGrab.reset(nullptr);

        const auto PTI = getFocusedTextInput();

        Debug::log(LOG, "IME Destroy");

        if (PTI) {
            setPendingSurface(PTI, PTI->pWlrInput->focused_surface);

            wlr_text_input_v3_send_leave(PTI->pWlrInput);
        }

    }, this, "IMERelay");

    hyprListener_IMEGrab.initCallback(&m_pWLRIME->events.grab_keyboard, [&](void* owner, void* data) {

        Debug::log(LOG, "IME TextInput Keyboard Grab new");

        m_pKeyboardGrab.reset(nullptr);

        m_pKeyboardGrab = std::make_unique<SIMEKbGrab>();

        m_pKeyboardGrab->pKeyboard = wlr_seat_get_keyboard(g_pCompositor->m_sSeat.seat);

        const auto PKBGRAB = (wlr_input_method_keyboard_grab_v2*)data;

        m_pKeyboardGrab->pWlrKbGrab = PKBGRAB;

        wlr_input_method_keyboard_grab_v2_set_keyboard(m_pKeyboardGrab->pWlrKbGrab, m_pKeyboardGrab->pKeyboard);

        m_pKeyboardGrab->hyprListener_grabDestroy.initCallback(&PKBGRAB->events.destroy, [&](void* owner, void* data) {

            m_pKeyboardGrab->hyprListener_grabDestroy.removeCallback();

            Debug::log(LOG, "IME TextInput Keyboard Grab destroy");

            if (m_pKeyboardGrab->pKeyboard) {
                wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sSeat.seat, &m_pKeyboardGrab->pKeyboard->modifiers);
            }

            m_pKeyboardGrab.reset(nullptr);

        }, m_pKeyboardGrab.get(), "IME Keyboard Grab");

    }, this, "IMERelay");

    const auto PTI = getFocusableTextInput();

    if (PTI) {
        wlr_text_input_v3_send_enter(PTI->pWlrInput, PTI->pPendingSurface);
        setPendingSurface(PTI, nullptr);
    }
}

SIMEKbGrab* CInputMethodRelay::getIMEKeyboardGrab(SKeyboard* pKeyboard) {

    if (!m_pWLRIME)
        return nullptr;

    if (!m_pKeyboardGrab.get())
        return nullptr;

    const auto VIRTKB = wlr_input_device_get_virtual_keyboard(pKeyboard->keyboard);

    if (VIRTKB && (wl_resource_get_client(VIRTKB->resource) == wl_resource_get_client(m_pKeyboardGrab->pWlrKbGrab->resource)))
        return nullptr;

    if (wlr_keyboard_from_input_device(pKeyboard->keyboard) != m_pKeyboardGrab->pKeyboard)
        return nullptr;

    return m_pKeyboardGrab.get();
}

STextInput* CInputMethodRelay::getFocusedTextInput() {
    for (auto& ti : m_lTextInputs) {
        if (ti.pWlrInput->focused_surface) {
            return &ti;
        }
    }

    return nullptr;
}

STextInput* CInputMethodRelay::getFocusableTextInput() {
    for (auto& ti : m_lTextInputs) {
        if (ti.pPendingSurface) {
            return &ti;
        }
    }

    return nullptr;
}

void CInputMethodRelay::onNewTextInput(wlr_text_input_v3* pInput) {
    createNewTextInput(pInput);
}

void CInputMethodRelay::createNewTextInput(wlr_text_input_v3* pInput) {
    const auto PTEXTINPUT = &m_lTextInputs.emplace_back();

    PTEXTINPUT->pWlrInput = pInput;

    PTEXTINPUT->hyprListener_textInputEnable.initCallback(&pInput->events.enable, [](void* owner, void* data) {

        const auto PINPUT = (STextInput*)owner;

        if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
            Debug::log(ERR, "Enabling TextInput on no IME!");
            return;
        }

        Debug::log(LOG, "Enable TextInput");

        wlr_input_method_v2_send_activate(g_pInputManager->m_sIMERelay.m_pWLRIME);
        g_pInputManager->m_sIMERelay.commitIMEState(PINPUT->pWlrInput);

    }, PTEXTINPUT, "textInput");

    PTEXTINPUT->hyprListener_textInputCommit.initCallback(&pInput->events.commit, [](void* owner, void* data) {

        const auto PINPUT = (STextInput*)owner;

        if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
            Debug::log(ERR, "Committing TextInput on no IME!");
            return;
        }

        if (!PINPUT->pWlrInput->current_enabled) {
            Debug::log(ERR, "Disabled TextInput commit?");
            return;
        }

        g_pInputManager->m_sIMERelay.commitIMEState(PINPUT->pWlrInput);

    }, PTEXTINPUT, "textInput");

    PTEXTINPUT->hyprListener_textInputDisable.initCallback(&pInput->events.disable, [](void* owner, void* data) {

        const auto PINPUT = (STextInput*)owner;

        if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
            Debug::log(ERR, "Disabling TextInput on no IME!");
            return;
        }

        Debug::log(LOG, "Disable TextInput");

        wlr_input_method_v2_send_deactivate(g_pInputManager->m_sIMERelay.m_pWLRIME);

        g_pInputManager->m_sIMERelay.commitIMEState(PINPUT->pWlrInput);

    }, PTEXTINPUT, "textInput");

    PTEXTINPUT->hyprListener_textInputDestroy.initCallback(&pInput->events.destroy, [](void* owner, void* data) {

        const auto PINPUT = (STextInput*)owner;

        if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
            Debug::log(ERR, "Disabling TextInput on no IME!");
            return;
        }

        if (PINPUT->pWlrInput->current_enabled) {
            wlr_input_method_v2_send_deactivate(g_pInputManager->m_sIMERelay.m_pWLRIME);

            g_pInputManager->m_sIMERelay.commitIMEState(PINPUT->pWlrInput);
        }

        g_pInputManager->m_sIMERelay.setPendingSurface(PINPUT, nullptr);

        PINPUT->hyprListener_textInputCommit.removeCallback();
        PINPUT->hyprListener_textInputDestroy.removeCallback();
        PINPUT->hyprListener_textInputDisable.removeCallback();
        PINPUT->hyprListener_textInputEnable.removeCallback();

        g_pInputManager->m_sIMERelay.removeTextInput(PINPUT->pWlrInput);

    }, PTEXTINPUT, "textInput");
}

void CInputMethodRelay::removeTextInput(wlr_text_input_v3* pInput) {
    m_lTextInputs.remove_if([&](const auto& other) { return other.pWlrInput == pInput; });
}

void CInputMethodRelay::commitIMEState(wlr_text_input_v3* pInput) {
    if (pInput->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT)
        wlr_input_method_v2_send_surrounding_text(m_pWLRIME, pInput->current.surrounding.text, pInput->current.surrounding.cursor, pInput->current.surrounding.anchor);

    wlr_input_method_v2_send_text_change_cause(m_pWLRIME, pInput->current.text_change_cause);

    if (pInput->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE)
        wlr_input_method_v2_send_content_type(m_pWLRIME, pInput->current.content_type.hint, pInput->current.content_type.purpose);

    wlr_input_method_v2_send_done(m_pWLRIME);
}

void CInputMethodRelay::onKeyboardFocus(wlr_surface* pSurface) {
    if (!m_pWLRIME)
        return;

    for (auto& ti : m_lTextInputs) {
        if (ti.pPendingSurface) {

            if (pSurface != ti.pPendingSurface) 
                setPendingSurface(&ti, nullptr);

        } else if (ti.pWlrInput->focused_surface) {

            if (pSurface != ti.pWlrInput->focused_surface) {
                wlr_input_method_v2_send_deactivate(m_pWLRIME);
                commitIMEState(ti.pWlrInput);

                wlr_text_input_v3_send_leave(ti.pWlrInput);
            } else {
                continue;
            }

        }

        if (pSurface && wl_resource_get_client(ti.pWlrInput->resource) == wl_resource_get_client(pSurface->resource)) {

            if (m_pWLRIME) {
                wlr_text_input_v3_send_enter(ti.pWlrInput, pSurface);
            } else {
                setPendingSurface(&ti, pSurface);
            }

        }
    }
}

void CInputMethodRelay::setPendingSurface(STextInput* pInput, wlr_surface* pSurface) {
    pInput->pPendingSurface = pSurface;

    if (pSurface) {
        pInput->hyprListener_pendingSurfaceDestroy.initCallback(&pSurface->events.destroy, [](void* owner, void* data) {
            const auto PINPUT = (STextInput*)owner;

            PINPUT->pPendingSurface = nullptr;

            PINPUT->hyprListener_pendingSurfaceDestroy.removeCallback();
        }, pInput, "TextInput");
    } else {
        pInput->hyprListener_pendingSurfaceDestroy.removeCallback();
    }
}