#include "TextInput.hpp"
#include "../../defines.hpp"
#include "InputManager.hpp"
#include "../../protocols/TextInputV1.hpp"
#include "../../Compositor.hpp"

CTextInput::CTextInput(STextInputV1* ti) : pV1Input(ti) {
    ti->pTextInput = this;
    initCallbacks();
}

CTextInput::CTextInput(wlr_text_input_v3* ti) : pWlrInput(ti) {
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
    hyprListener_textInputEnable.initCallback(
        isV3() ? &pWlrInput->events.enable : &pV1Input->sEnable, [this](void* owner, void* data) { onEnabled(); }, this, "textInput");

    hyprListener_textInputCommit.initCallback(
        isV3() ? &pWlrInput->events.commit : &pV1Input->sCommit, [this](void* owner, void* data) { onCommit(); }, this, "textInput");

    hyprListener_textInputDisable.initCallback(
        isV3() ? &pWlrInput->events.disable : &pV1Input->sDisable, [this](void* owner, void* data) { onDisabled(); }, this, "textInput");

    hyprListener_textInputDestroy.initCallback(
        isV3() ? &pWlrInput->events.destroy : &pV1Input->sDestroy,
        [this](void* owner, void* data) {
            if (pWlrInput && pWlrInput->current_enabled && focusedSurface())
                g_pInputManager->m_sIMERelay.deactivateIME(this);

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

    if (!(pWlrInput ? pWlrInput->current_enabled : pV1Input->active)) {
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
    return pWlrInput;
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

    if (pWlrInput)
        wlr_text_input_v3_send_enter(pWlrInput, pSurface);
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

    if (pWlrInput && pWlrInput->focused_surface)
        wlr_text_input_v3_send_leave(pWlrInput);
    else if (focusedSurface() && pV1Input) {
        zwp_text_input_v1_send_leave(pV1Input->resourceImpl);
        pV1Input->active = false;
    }

    setFocusedSurface(nullptr);

    g_pInputManager->m_sIMERelay.deactivateIME(this);
}

wlr_surface* CTextInput::focusedSurface() {
    return pWlrInput ? pWlrInput->focused_surface : pFocusedSurface;
}

wl_client* CTextInput::client() {
    return pWlrInput ? wl_resource_get_client(pWlrInput->resource) : pV1Input->client;
}

void CTextInput::commitStateToIME(wlr_input_method_v2* ime) {
    if (isV3()) {
        if (pWlrInput->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT)
            wlr_input_method_v2_send_surrounding_text(ime, pWlrInput->current.surrounding.text, pWlrInput->current.surrounding.cursor, pWlrInput->current.surrounding.anchor);

        wlr_input_method_v2_send_text_change_cause(ime, pWlrInput->current.text_change_cause);

        if (pWlrInput->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE)
            wlr_input_method_v2_send_content_type(ime, pWlrInput->current.content_type.hint, pWlrInput->current.content_type.purpose);
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
        if (ime->current.preedit.text) {
            wlr_text_input_v3_send_preedit_string(pWlrInput, ime->current.preedit.text, ime->current.preedit.cursor_begin, ime->current.preedit.cursor_end);
        }

        if (ime->current.commit_text) {
            wlr_text_input_v3_send_commit_string(pWlrInput, ime->current.commit_text);
        }

        if (ime->current.delete_.before_length || ime->current.delete_.after_length) {
            wlr_text_input_v3_send_delete_surrounding_text(pWlrInput, ime->current.delete_.before_length, ime->current.delete_.after_length);
        }

        wlr_text_input_v3_send_done(pWlrInput);
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
    return !isV3() || pWlrInput->current.features & WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE;
}

CBox CTextInput::cursorBox() {
    return CBox{isV3() ? pWlrInput->current.cursor_rectangle : pV1Input->cursorRectangle};
}