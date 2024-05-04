#include "TextInput.hpp"
#include "../../defines.hpp"
#include "InputManager.hpp"
#include "../../protocols/TextInputV1.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/TextInputV3.hpp"
#include "../../protocols/InputMethodV2.hpp"

CTextInput::CTextInput(STextInputV1* ti) : pV1Input(ti) {
    ti->pTextInput = this;
    initCallbacks();
}

CTextInput::CTextInput(WP<CTextInputV3> ti) : pV3Input(ti) {
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

    if (g_pInputManager->m_sIMERelay.m_pIME.expired()) {
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
    if (g_pInputManager->m_sIMERelay.m_pIME.expired()) {
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
    if (g_pInputManager->m_sIMERelay.m_pIME.expired()) {
        //   Debug::log(WARN, "Committing TextInput on no IME!");
        return;
    }

    if (!(isV3() ? pV3Input->current.enabled : pV1Input->active)) {
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
        pV3Input->enter(pSurface);
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
        pV3Input->leave(focusedSurface());
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
    return isV3() ? pV3Input->client() : pV1Input->client;
}

void CTextInput::commitStateToIME(SP<CInputMethodV2> ime) {
    if (isV3()) {
        const auto INPUT = pV3Input.lock();

        if (INPUT->current.surrounding.updated)
            ime->surroundingText(INPUT->current.surrounding.text, INPUT->current.surrounding.cursor, INPUT->current.surrounding.anchor);

        ime->textChangeCause(INPUT->current.cause);

        if (INPUT->current.contentType.updated)
            ime->textContentType(INPUT->current.contentType.hint, INPUT->current.contentType.purpose);
    } else {
        if (pV1Input->pendingSurrounding.isPending)
            ime->surroundingText(pV1Input->pendingSurrounding.text, pV1Input->pendingSurrounding.cursor, pV1Input->pendingSurrounding.anchor);

        ime->textChangeCause(ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);

        if (pV1Input->pendingContentType.isPending)
            ime->textContentType((zwpTextInputV3ContentHint)pV1Input->pendingContentType.hint, (zwpTextInputV3ContentPurpose)pV1Input->pendingContentType.purpose);
    }

    g_pInputManager->m_sIMERelay.updateAllPopups();

    ime->done();
}

void CTextInput::updateIMEState(SP<CInputMethodV2> ime) {
    if (isV3()) {
        const auto INPUT = pV3Input.lock();

        if (ime->current.preeditString.committed)
            INPUT->preeditString(ime->current.preeditString.string, ime->current.preeditString.begin, ime->current.preeditString.end);

        if (ime->current.committedString.committed)
            INPUT->commitString(ime->current.committedString.string);

        if (ime->current.deleteSurrounding.committed)
            INPUT->deleteSurroundingText(ime->current.deleteSurrounding.before, ime->current.deleteSurrounding.after);

        INPUT->sendDone();
    } else {
        if (ime->current.preeditString.committed) {
            zwp_text_input_v1_send_preedit_cursor(pV1Input->resourceImpl, ime->current.preeditString.begin);
            zwp_text_input_v1_send_preedit_styling(pV1Input->resourceImpl, 0, std::string(ime->current.preeditString.string).length(), ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
            zwp_text_input_v1_send_preedit_string(pV1Input->resourceImpl, pV1Input->serial, ime->current.preeditString.string.c_str(), "");
        } else {
            zwp_text_input_v1_send_preedit_cursor(pV1Input->resourceImpl, ime->current.preeditString.begin);
            zwp_text_input_v1_send_preedit_styling(pV1Input->resourceImpl, 0, 0, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
            zwp_text_input_v1_send_preedit_string(pV1Input->resourceImpl, pV1Input->serial, "", "");
        }

        if (ime->current.committedString.committed)
            zwp_text_input_v1_send_commit_string(pV1Input->resourceImpl, pV1Input->serial, ime->current.committedString.string.c_str());

        if (ime->current.deleteSurrounding.committed) {
            zwp_text_input_v1_send_delete_surrounding_text(pV1Input->resourceImpl, std::string(ime->current.preeditString.string).length() - ime->current.deleteSurrounding.before,
                                                           ime->current.deleteSurrounding.after + ime->current.deleteSurrounding.before);

            if (ime->current.preeditString.committed)
                zwp_text_input_v1_send_commit_string(pV1Input->resourceImpl, pV1Input->serial, ime->current.preeditString.string.c_str());
        }
    }
}

bool CTextInput::hasCursorRectangle() {
    return !isV3() || pV3Input.lock()->current.box.updated;
}

CBox CTextInput::cursorBox() {
    return CBox{isV3() ? pV3Input.lock()->current.box.cursorBox : pV1Input->cursorRectangle};
}