#include "TextInput.hpp"
#include "../../defines.hpp"
#include "InputManager.hpp"
#include "../../protocols/TextInputV1.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/TextInputV3.hpp"
#include "../../protocols/InputMethodV2.hpp"
#include "../../protocols/core/Compositor.hpp"

CTextInput::CTextInput(WP<CTextInputV1> ti) : pV1Input(ti) {
    initCallbacks();
}

CTextInput::CTextInput(WP<CTextInputV3> ti) : pV3Input(ti) {
    initCallbacks();
}

void CTextInput::initCallbacks() {
    if (isV3()) {
        const auto INPUT = pV3Input.lock();

        listeners.enable  = INPUT->events.enable.registerListener([this](std::any p) { onEnabled(); });
        listeners.disable = INPUT->events.disable.registerListener([this](std::any p) { onDisabled(); });
        listeners.commit  = INPUT->events.onCommit.registerListener([this](std::any p) { onCommit(); });
        listeners.reset   = INPUT->events.reset.registerListener([this](std::any p) { onReset(); });
        listeners.destroy = INPUT->events.destroy.registerListener([this](std::any p) {
            listeners.surfaceUnmap.reset();
            listeners.surfaceDestroy.reset();
            g_pInputManager->m_sIMERelay.removeTextInput(this);
            if (!g_pInputManager->m_sIMERelay.getFocusedTextInput())
                g_pInputManager->m_sIMERelay.deactivateIME(this);
        });

        if (!g_pCompositor->m_pLastFocus.expired() && g_pCompositor->m_pLastFocus->client() == INPUT->client())
            enter(g_pCompositor->m_pLastFocus.lock());
    } else {
        const auto INPUT = pV1Input.lock();

        listeners.enable  = INPUT->events.enable.registerListener([this](std::any p) {
            const auto SURFACE = std::any_cast<SP<CWLSurfaceResource>>(p);
            onEnabled(SURFACE);
        });
        listeners.disable = INPUT->events.disable.registerListener([this](std::any p) { onDisabled(); });
        listeners.commit  = INPUT->events.onCommit.registerListener([this](std::any p) { onCommit(); });
        listeners.reset   = INPUT->events.reset.registerListener([this](std::any p) { onReset(); });
        listeners.destroy = INPUT->events.destroy.registerListener([this](std::any p) {
            listeners.surfaceUnmap.reset();
            listeners.surfaceDestroy.reset();
            g_pInputManager->m_sIMERelay.removeTextInput(this);
            if (!g_pInputManager->m_sIMERelay.getFocusedTextInput())
                g_pInputManager->m_sIMERelay.deactivateIME(this);
        });
    }
}

void CTextInput::onEnabled(SP<CWLSurfaceResource> surfV1) {
    NDebug::log(LOG, "TI ENABLE");

    if (g_pInputManager->m_sIMERelay.m_pIME.expired()) {
        // NDebug::log(WARN, "Enabling TextInput on no IME!");
        return;
    }

    // v1 only, map surface to PTI
    if (!isV3()) {
        if (g_pCompositor->m_pLastFocus != surfV1 || !pV1Input->active)
            return;

        enter(surfV1);
    }

    g_pInputManager->m_sIMERelay.activateIME(this);
}

void CTextInput::onDisabled() {
    if (g_pInputManager->m_sIMERelay.m_pIME.expired()) {
        //  NDebug::log(WARN, "Disabling TextInput on no IME!");
        return;
    }

    if (!isV3())
        leave();

    listeners.surfaceUnmap.reset();
    listeners.surfaceDestroy.reset();

    if (!focusedSurface())
        return;

    const auto PFOCUSEDTI = g_pInputManager->m_sIMERelay.getFocusedTextInput();
    if (!PFOCUSEDTI || PFOCUSEDTI != this)
        return;

    g_pInputManager->m_sIMERelay.deactivateIME(this);
}

void CTextInput::onReset() {
    if (g_pInputManager->m_sIMERelay.m_pIME.expired())
        return;

    if (!focusedSurface())
        return;

    const auto PFOCUSEDTI = g_pInputManager->m_sIMERelay.getFocusedTextInput();
    if (!PFOCUSEDTI || PFOCUSEDTI != this)
        return;

    g_pInputManager->m_sIMERelay.deactivateIME(this, false);
    g_pInputManager->m_sIMERelay.activateIME(this);
}

void CTextInput::onCommit() {
    if (g_pInputManager->m_sIMERelay.m_pIME.expired()) {
        //   NDebug::log(WARN, "Committing TextInput on no IME!");
        return;
    }

    if (!(isV3() ? pV3Input->current.enabled.value : pV1Input->active)) {
        NDebug::log(WARN, "Disabled TextInput commit?");
        return;
    }

    g_pInputManager->m_sIMERelay.commitIMEState(this);
}

void CTextInput::setFocusedSurface(SP<CWLSurfaceResource> pSurface) {
    if (pSurface == pFocusedSurface)
        return;

    pFocusedSurface = pSurface;

    if (!pSurface)
        return;

    listeners.surfaceUnmap.reset();
    listeners.surfaceDestroy.reset();

    listeners.surfaceUnmap = pSurface->events.unmap.registerListener([this](std::any d) {
        NDebug::log(LOG, "Unmap TI owner1");

        if (enterLocks)
            enterLocks--;
        pFocusedSurface.reset();
        listeners.surfaceUnmap.reset();
        listeners.surfaceDestroy.reset();

        if (isV3() && !pV3Input.expired() && pV3Input->current.enabled.value) {
            pV3Input->pending.enabled.value            = false;
            pV3Input->pending.enabled.isDisablePending = false;
            pV3Input->pending.enabled.isEnablePending  = false;
            pV3Input->current.enabled.value            = false;
        }

        if (!g_pInputManager->m_sIMERelay.getFocusedTextInput())
            g_pInputManager->m_sIMERelay.deactivateIME(this);
    });

    listeners.surfaceDestroy = pSurface->events.destroy.registerListener([this](std::any d) {
        NDebug::log(LOG, "Destroy TI owner1");

        if (enterLocks)
            enterLocks--;
        pFocusedSurface.reset();
        listeners.surfaceUnmap.reset();
        listeners.surfaceDestroy.reset();

        if (isV3() && !pV3Input.expired() && pV3Input->current.enabled.value) {
            pV3Input->pending.enabled.value            = false;
            pV3Input->pending.enabled.isDisablePending = false;
            pV3Input->pending.enabled.isEnablePending  = false;
            pV3Input->current.enabled.value            = false;
        }

        if (!g_pInputManager->m_sIMERelay.getFocusedTextInput())
            g_pInputManager->m_sIMERelay.deactivateIME(this);
    });
}

bool CTextInput::isV3() {
    return pV3Input && !pV1Input;
}

void CTextInput::enter(SP<CWLSurfaceResource> pSurface) {
    if (!pSurface || !pSurface->mapped)
        return;

    if (pSurface == focusedSurface())
        return;

    if (focusedSurface())
        leave();

    enterLocks++;
    if (enterLocks != 1) {
        NDebug::log(ERR, "BUG THIS: TextInput has != 1 locks in enter");
        leave();
        enterLocks = 1;
    }

    if (isV3())
        pV3Input->enter(pSurface);
    else {
        pV1Input->enter(pSurface);
    }

    setFocusedSurface(pSurface);
}

void CTextInput::leave() {
    if (!focusedSurface())
        return;

    enterLocks--;
    if (enterLocks != 0) {
        NDebug::log(ERR, "BUG THIS: TextInput has != 0 locks in leave");
        enterLocks = 0;
    }

    if (isV3())
        pV3Input->leave(focusedSurface());
    else
        pV1Input->leave();

    setFocusedSurface(nullptr);

    g_pInputManager->m_sIMERelay.deactivateIME(this);
}

SP<CWLSurfaceResource> CTextInput::focusedSurface() {
    return pFocusedSurface.lock();
}

wl_client* CTextInput::client() {
    return isV3() ? pV3Input->client() : pV1Input->client();
}

void CTextInput::commitStateToIME(SP<CInputMethodV2> ime) {
    if (isV3() && !pV3Input.expired()) {
        const auto INPUT = pV3Input.lock();

        if (INPUT->current.surrounding.updated)
            ime->surroundingText(INPUT->current.surrounding.text, INPUT->current.surrounding.cursor, INPUT->current.surrounding.anchor);

        ime->textChangeCause(INPUT->current.cause);

        if (INPUT->current.contentType.updated)
            ime->textContentType(INPUT->current.contentType.hint, INPUT->current.contentType.purpose);
    } else if (!pV1Input.expired()) {
        const auto INPUT = pV1Input.lock();

        if (INPUT->pendingSurrounding.isPending)
            ime->surroundingText(INPUT->pendingSurrounding.text, INPUT->pendingSurrounding.cursor, INPUT->pendingSurrounding.anchor);

        ime->textChangeCause(ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);

        if (pV1Input->pendingContentType.isPending)
            ime->textContentType((zwpTextInputV3ContentHint)INPUT->pendingContentType.hint, (zwpTextInputV3ContentPurpose)INPUT->pendingContentType.purpose);
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
        const auto INPUT = pV1Input.lock();

        if (ime->current.preeditString.committed) {
            INPUT->preeditCursor(ime->current.preeditString.begin);
            INPUT->preeditStyling(0, std::string(ime->current.preeditString.string).length(), ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
            INPUT->preeditString(pV1Input->serial, ime->current.preeditString.string.c_str(), "");
        } else {
            INPUT->preeditCursor(0);
            INPUT->preeditStyling(0, 0, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
            INPUT->preeditString(pV1Input->serial, "", "");
        }

        if (ime->current.committedString.committed)
            INPUT->commitString(pV1Input->serial, ime->current.committedString.string.c_str());

        if (ime->current.deleteSurrounding.committed) {
            INPUT->deleteSurroundingText(std::string(ime->current.preeditString.string).length() - ime->current.deleteSurrounding.before,
                                         ime->current.deleteSurrounding.after + ime->current.deleteSurrounding.before);

            if (ime->current.preeditString.committed)
                INPUT->commitString(pV1Input->serial, ime->current.preeditString.string.c_str());
        }
    }
}

bool CTextInput::hasCursorRectangle() {
    return !isV3() || pV3Input->current.box.updated;
}

CBox CTextInput::cursorBox() {
    return CBox{isV3() ? pV3Input->current.box.cursorBox : pV1Input->cursorRectangle};
}
