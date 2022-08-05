#include "InputMethodRelay.hpp"
#include "InputManager.hpp"

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
            wlr_text_input_v3_send_preedit_string(PTI->pWlrInput, PIMR->m_pWLRIME->current.preedit.text, PIMR->m_pWLRIME->current.preedit.cursor_begin, PIMR->m_pWLRIME->current.preedit.cursor_end);
        }

        if (PIMR->m_pWLRIME->current.commit_text) {
            wlr_text_input_v3_send_commit_string(PTI->pWlrInput, PIMR->m_pWLRIME->current.commit_text);
        }

        if (PIMR->m_pWLRIME->current.delete_.before_length ||
            PIMR->m_pWLRIME->current.delete_.after_length) {
            wlr_text_input_v3_send_delete_surrounding_text(PTI->pWlrInput, PIMR->m_pWLRIME->current.delete_.before_length, PIMR->m_pWLRIME->current.delete_.after_length);
        }

        wlr_text_input_v3_send_done(PTI->pWlrInput);

    }, this, "IMERelay");

    hyprListener_IMEDestroy.initCallback(&m_pWLRIME->events.destroy, [&](void* owner, void* data) {

        m_pWLRIME = nullptr;

        hyprListener_IMEDestroy.removeCallback();
        hyprListener_IMECommit.removeCallback();

        const auto PTI = getFocusedTextInput();

        Debug::log(LOG, "IME Destroy");

        if (PTI) {
            PTI->pPendingSurface = PTI->pWlrInput->focused_surface;

            wlr_text_input_v3_send_leave(PTI->pWlrInput);
        }

    }, this, "IMERelay");

    const auto PTI = getFocusableTextInput();

    if (PTI) {
        wlr_text_input_v3_send_enter(PTI->pWlrInput, PTI->pPendingSurface);
        PTI->pPendingSurface = nullptr;
    }
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
    wlr_input_method_v2_send_surrounding_text(m_pWLRIME, pInput->current.surrounding.text, pInput->current.surrounding.cursor, pInput->current.surrounding.anchor);
    wlr_input_method_v2_send_text_change_cause(m_pWLRIME, pInput->current.text_change_cause);
    wlr_input_method_v2_send_content_type(m_pWLRIME, pInput->current.content_type.hint, pInput->current.content_type.purpose);
    wlr_input_method_v2_send_done(m_pWLRIME);
}

void CInputMethodRelay::onKeyboardFocus(wlr_surface* pSurface) {
    if (!m_pWLRIME)
        return;

    for (auto& ti : m_lTextInputs) {
        if (ti.pPendingSurface) {

            if (pSurface != ti.pPendingSurface) 
                ti.pPendingSurface = nullptr;

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
                ti.pPendingSurface = pSurface;
            }

        }
    }
}