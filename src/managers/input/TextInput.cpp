#include "TextInput.hpp"
#include "../../defines.hpp"
#include "InputManager.hpp"
#include "../../protocols/TextInputV1.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/TextInputV3.hpp"

CTextInput::CTextInput(STextInputV1* ti) : pV1Input(ti) {
    ti->pTextInput = this;
    initCallbacks();
}

CTextInput::CTextInput(std::weak_ptr<CTextInputV3> ti) : pV3Input(ti) {
    initCallbacks();
}

CTextInput::~CTextInput() {
    if (pV1Input)
        pV1Input->pTextInput = nullptr;
}

void CTextInput::tiV1Destroyed() {
    pV1Input = nullptr;

    g_pInputManager->m_sIMERelay.removeTextInput(this);
}

void CTextInput::initCallbacks() {
    if (isV3()) {
        const auto INPUT = pV3Input.lock();

        listeners.enable  = INPUT->events.enable.registerListener([this](std::any p) { onEnabled(); });
        listeners.disable = INPUT->events.disable.registerListener([this](std::any p) { onDisabled(); });
        listeners.commit  = INPUT->events.onCommit.registerListener([this](std::any p) { onCommit(); });
        listeners.destroy = INPUT->events.destroy.registerListener([this](std::any p) {
            const auto INPUT = pV3Input.lock();
            if (INPUT && INPUT->current.enabled && focusedSurface())
                g_pInputManager->m_sIMERelay.deactivateIME(this);
            g_pInputManager->m_sIMERelay.removeTextInput(this);
        });
    } else {
        hyprListener_textInputEnable.initCallback(
            &pV1Input->sEnable, [this](void* owner, void* data) { onEnabled(); }, this, "textInput");

        hyprListener_textInputCommit.initCallback(
            &pV1Input->sCommit, [this](void* owner, void* data) { onCommit(); }, this, "textInput");

        hyprListener_textInputDisable.initCallback(
            &pV1Input->sDisable, [this](void* owner, void* data) { onDisabled(); }, this, "textInput");

        hyprListener_textInputDestroy.initCallback(
            &pV1Input->sDestroy,
            [this](void* owner, void* data) {
                hyprListener_textInputCommit.removeCallback();
                hyprListener_textInputDestroy.removeCallback();
                hyprListener_textInputDisable.removeCallback();
                hyprListener_textInputEnable.removeCallback();
                hyprListener_surfaceDestroyed.removeCallback();
                hyprListener_surfaceUnmapped.removeCallback();

                g_pInputManager->m_sIMERelay.removeTextInput(this);
            },
            this, "textInput");
    }
}

void CTextInput::onEnabled(wlr_surface* surfV1) {
    Debug::log(LOG, "TI ENABLE");

    if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
        // Debug::log(WARN, "Enabling TextInput on no IME!");
        return;
    }

    // v1 only, map surface to PTI
    if (!isV3()) {
        wlr_surface* pSurface = surfV1;
        if (g_pCompositor->m_pLastFocus != pSurface || !pV1Input->active)
            return;

        enter(pSurface);
    }

    g_pInputManager->m_sIMERelay.activateIME(this);
}

void CTextInput::onDisabled() {
    if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
        //  Debug::log(WARN, "Disabling TextInput on no IME!");
        return;
    }

    if (!focusedSurface())
        return;

    if (!isV3())
        leave();

    hyprListener_surfaceDestroyed.removeCallback();
    hyprListener_surfaceUnmapped.removeCallback();

    g_pInputManager->m_sIMERelay.deactivateIME(this);
}

void CTextInput::onCommit() {
    if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
        //   Debug::log(WARN, "Committing TextInput on no IME!");
        return;
    }

    if (!(isV3() ? pV3Input.lock()->current.enabled : pV1Input->active)) {
        Debug::log(WARN, "Disabled TextInput commit?");
        return;
    }

    g_pInputManager->m_sIMERelay.commitIMEState(this);
}

void CTextInput::setFocusedSurface(wlr_surface* pSurface) {
    if (pSurface == pFocusedSurface)
        return;

    pFocusedSurface = pSurface;

    hyprListener_surfaceUnmapped.removeCallback();
    hyprListener_surfaceDestroyed.removeCallback();

    if (!pSurface)
        return;

    hyprListener_surfaceUnmapped.initCallback(
        &pSurface->events.unmap,
        [this](void* owner, void* data) {
            Debug::log(LOG, "Unmap TI owner1");

            if (enterLocks)
                enterLocks--;
            pFocusedSurface = nullptr;
            hyprListener_surfaceUnmapped.removeCallback();
            hyprListener_surfaceDestroyed.removeCallback();
        },
        this, "CTextInput");

    hyprListener_surfaceDestroyed.initCallback(
        &pSurface->events.destroy,
        [this](void* owner, void* data) {
            Debug::log(LOG, "destroy TI owner1");

            if (enterLocks)
                enterLocks--;
            pFocusedSurface = nullptr;
            hyprListener_surfaceUnmapped.removeCallback();
            hyprListener_surfaceDestroyed.removeCallback();
        },
        this, "CTextInput");
}

bool CTextInput::isV3() {
    return !pV1Input;
}

void CTextInput::enter(wlr_surface* pSurface) {
    if (!pSurface || !pSurface->mapped)
        return;

    if (pSurface == focusedSurface())
        return;

    if (focusedSurface()) {
        leave();
        setFocusedSurface(nullptr);
    }

    enterLocks++;
    if (enterLocks != 1) {
        Debug::log(ERR, "BUG THIS: TextInput has != 1 locks in enter");
        leave();
        enterLocks = 1;
    }

    if (isV3())
        pV3Input.lock()->enter(pSurface);
    else {
        zwp_text_input_v1_send_enter(pV1Input->resourceImpl, pSurface->resource);
        pV1Input->active = true;
    }

    setFocusedSurface(pSurface);
}

void CTextInput::leave() {
    if (!focusedSurface())
        return;

    enterLocks--;
    if (enterLocks != 0) {
        Debug::log(ERR, "BUG THIS: TextInput has != 0 locks in leave");
        enterLocks = 0;
    }

    if (isV3() && focusedSurface())
        pV3Input.lock()->leave(focusedSurface());
    else if (focusedSurface() && pV1Input) {
        zwp_text_input_v1_send_leave(pV1Input->resourceImpl);
        pV1Input->active = false;
    }

    setFocusedSurface(nullptr);

    g_pInputManager->m_sIMERelay.deactivateIME(this);
}

wlr_surface* CTextInput::focusedSurface() {
    return pFocusedSurface;
}

wl_client* CTextInput::client() {
    return isV3() ? pV3Input.lock()->client() : pV1Input->client;
}

void CTextInput::commitStateToIME(wlr_input_method_v2* ime) {
    if (isV3()) {
        const auto INPUT = pV3Input.lock();

        if (INPUT->current.surrounding.updated)
            wlr_input_method_v2_send_surrounding_text(ime, INPUT->current.surrounding.text.c_str(), INPUT->current.surrounding.cursor, INPUT->current.surrounding.anchor);

        wlr_input_method_v2_send_text_change_cause(ime, INPUT->current.cause);

        if (INPUT->current.contentType.updated)
            wlr_input_method_v2_send_content_type(ime, INPUT->current.contentType.hint, INPUT->current.contentType.purpose);
    } else {
        if (pV1Input->pendingSurrounding.isPending)
            wlr_input_method_v2_send_surrounding_text(ime, pV1Input->pendingSurrounding.text.c_str(), pV1Input->pendingSurrounding.cursor, pV1Input->pendingSurrounding.anchor);

        wlr_input_method_v2_send_text_change_cause(ime, 0);

        if (pV1Input->pendingContentType.isPending)
            wlr_input_method_v2_send_content_type(ime, pV1Input->pendingContentType.hint, pV1Input->pendingContentType.purpose);
    }

    g_pInputManager->m_sIMERelay.updateAllPopups();

    wlr_input_method_v2_send_done(ime);
}

void CTextInput::updateIMEState(wlr_input_method_v2* ime) {
    if (isV3()) {
        const auto INPUT = pV3Input.lock();

        if (ime->current.preedit.text)
            INPUT->preeditString(ime->current.preedit.text, ime->current.preedit.cursor_begin, ime->current.preedit.cursor_end);

        if (ime->current.commit_text)
            INPUT->commitString(ime->current.commit_text);

        if (ime->current.delete_.before_length || ime->current.delete_.after_length)
            INPUT->deleteSurroundingText(ime->current.delete_.before_length, ime->current.delete_.after_length);

        INPUT->sendDone();
    } else {
        if (ime->current.preedit.text) {
            zwp_text_input_v1_send_preedit_cursor(pV1Input->resourceImpl, ime->current.preedit.cursor_begin);
            zwp_text_input_v1_send_preedit_styling(pV1Input->resourceImpl, 0, std::string(ime->current.preedit.text).length(), ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
            zwp_text_input_v1_send_preedit_string(pV1Input->resourceImpl, pV1Input->serial, ime->current.preedit.text, "");
        } else {
            zwp_text_input_v1_send_preedit_cursor(pV1Input->resourceImpl, ime->current.preedit.cursor_begin);
            zwp_text_input_v1_send_preedit_styling(pV1Input->resourceImpl, 0, 0, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
            zwp_text_input_v1_send_preedit_string(pV1Input->resourceImpl, pV1Input->serial, "", "");
        }

        if (ime->current.commit_text) {
            zwp_text_input_v1_send_commit_string(pV1Input->resourceImpl, pV1Input->serial, ime->current.commit_text);
        }

        if (ime->current.delete_.before_length || ime->current.delete_.after_length) {
            zwp_text_input_v1_send_delete_surrounding_text(pV1Input->resourceImpl, std::string(ime->current.preedit.text).length() - ime->current.delete_.before_length,
                                                           ime->current.delete_.after_length + ime->current.delete_.before_length);

            if (ime->current.preedit.text)
                zwp_text_input_v1_send_commit_string(pV1Input->resourceImpl, pV1Input->serial, ime->current.preedit.text);
        }
    }
}

bool CTextInput::hasCursorRectangle() {
    return !isV3() || pV3Input.lock()->current.box.updated;
}

CBox CTextInput::cursorBox() {
    return CBox{isV3() ? pV3Input.lock()->current.box.cursorBox : pV1Input->cursorRectangle};
}