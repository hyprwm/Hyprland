#include "TextInput.hpp"
#include "../../defines.hpp"
#include "InputManager.hpp"
#include "../../protocols/TextInputV1.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/TextInputV3.hpp"
#include "../../protocols/InputMethodV2.hpp"
#include "../../protocols/core/Compositor.hpp"

CTextInput::CTextInput(WP<CTextInputV1> ti) : m_v1Input(ti) {
    initCallbacks();
}

CTextInput::CTextInput(WP<CTextInputV3> ti) : m_v3Input(ti) {
    initCallbacks();
}

void CTextInput::initCallbacks() {
    if (isV3()) {
        const auto INPUT = m_v3Input.lock();

        m_listeners.enable  = INPUT->events.enable.registerListener([this](std::any p) { onEnabled(); });
        m_listeners.disable = INPUT->events.disable.registerListener([this](std::any p) { onDisabled(); });
        m_listeners.commit  = INPUT->events.onCommit.registerListener([this](std::any p) { onCommit(); });
        m_listeners.reset   = INPUT->events.reset.registerListener([this](std::any p) { onReset(); });
        m_listeners.destroy = INPUT->events.destroy.registerListener([this](std::any p) {
            m_listeners.surfaceUnmap.reset();
            m_listeners.surfaceDestroy.reset();
            g_pInputManager->m_relay.removeTextInput(this);
            if (!g_pInputManager->m_relay.getFocusedTextInput())
                g_pInputManager->m_relay.deactivateIME(this);
        });

        if (!g_pCompositor->m_lastFocus.expired() && g_pCompositor->m_lastFocus->client() == INPUT->client())
            enter(g_pCompositor->m_lastFocus.lock());
    } else {
        const auto INPUT = m_v1Input.lock();

        m_listeners.enable  = INPUT->events.enable.registerListener([this](std::any p) {
            const auto SURFACE = std::any_cast<SP<CWLSurfaceResource>>(p);
            onEnabled(SURFACE);
        });
        m_listeners.disable = INPUT->events.disable.registerListener([this](std::any p) { onDisabled(); });
        m_listeners.commit  = INPUT->events.onCommit.registerListener([this](std::any p) { onCommit(); });
        m_listeners.reset   = INPUT->events.reset.registerListener([this](std::any p) { onReset(); });
        m_listeners.destroy = INPUT->events.destroy.registerListener([this](std::any p) {
            m_listeners.surfaceUnmap.reset();
            m_listeners.surfaceDestroy.reset();
            g_pInputManager->m_relay.removeTextInput(this);
            if (!g_pInputManager->m_relay.getFocusedTextInput())
                g_pInputManager->m_relay.deactivateIME(this);
        });
    }
}

void CTextInput::onEnabled(SP<CWLSurfaceResource> surfV1) {
    Debug::log(LOG, "TI ENABLE");

    if (g_pInputManager->m_relay.m_inputMethod.expired()) {
        // Debug::log(WARN, "Enabling TextInput on no IME!");
        return;
    }

    // v1 only, map surface to PTI
    if (!isV3()) {
        if (g_pCompositor->m_lastFocus != surfV1 || !m_v1Input->active)
            return;

        enter(surfV1);
    }

    g_pInputManager->m_relay.activateIME(this);
}

void CTextInput::onDisabled() {
    if (g_pInputManager->m_relay.m_inputMethod.expired()) {
        //  Debug::log(WARN, "Disabling TextInput on no IME!");
        return;
    }

    if (!isV3())
        leave();

    m_listeners.surfaceUnmap.reset();
    m_listeners.surfaceDestroy.reset();

    if (!focusedSurface())
        return;

    const auto PFOCUSEDTI = g_pInputManager->m_relay.getFocusedTextInput();
    if (!PFOCUSEDTI || PFOCUSEDTI != this)
        return;

    g_pInputManager->m_relay.deactivateIME(this);
}

void CTextInput::onReset() {
    if (g_pInputManager->m_relay.m_inputMethod.expired())
        return;

    if (!focusedSurface())
        return;

    const auto PFOCUSEDTI = g_pInputManager->m_relay.getFocusedTextInput();
    if (!PFOCUSEDTI || PFOCUSEDTI != this)
        return;

    g_pInputManager->m_relay.deactivateIME(this, false);
    g_pInputManager->m_relay.activateIME(this);
}

void CTextInput::onCommit() {
    if (g_pInputManager->m_relay.m_inputMethod.expired()) {
        //   Debug::log(WARN, "Committing TextInput on no IME!");
        return;
    }

    if (!(isV3() ? m_v3Input->current.enabled.value : m_v1Input->active)) {
        Debug::log(WARN, "Disabled TextInput commit?");
        return;
    }

    g_pInputManager->m_relay.commitIMEState(this);
}

void CTextInput::setFocusedSurface(SP<CWLSurfaceResource> pSurface) {
    if (pSurface == m_focusedSurface)
        return;

    m_focusedSurface = pSurface;

    if (!pSurface)
        return;

    m_listeners.surfaceUnmap.reset();
    m_listeners.surfaceDestroy.reset();

    m_listeners.surfaceUnmap = pSurface->m_events.unmap.registerListener([this](std::any d) {
        Debug::log(LOG, "Unmap TI owner1");

        if (m_enterLocks)
            m_enterLocks--;
        m_focusedSurface.reset();
        m_listeners.surfaceUnmap.reset();
        m_listeners.surfaceDestroy.reset();

        if (isV3() && !m_v3Input.expired() && m_v3Input->current.enabled.value) {
            m_v3Input->pending.enabled.value            = false;
            m_v3Input->pending.enabled.isDisablePending = false;
            m_v3Input->pending.enabled.isEnablePending  = false;
            m_v3Input->current.enabled.value            = false;
        }

        if (!g_pInputManager->m_relay.getFocusedTextInput())
            g_pInputManager->m_relay.deactivateIME(this);
    });

    m_listeners.surfaceDestroy = pSurface->m_events.destroy.registerListener([this](std::any d) {
        Debug::log(LOG, "Destroy TI owner1");

        if (m_enterLocks)
            m_enterLocks--;
        m_focusedSurface.reset();
        m_listeners.surfaceUnmap.reset();
        m_listeners.surfaceDestroy.reset();

        if (isV3() && !m_v3Input.expired() && m_v3Input->current.enabled.value) {
            m_v3Input->pending.enabled.value            = false;
            m_v3Input->pending.enabled.isDisablePending = false;
            m_v3Input->pending.enabled.isEnablePending  = false;
            m_v3Input->current.enabled.value            = false;
        }

        if (!g_pInputManager->m_relay.getFocusedTextInput())
            g_pInputManager->m_relay.deactivateIME(this);
    });
}

bool CTextInput::isV3() {
    return m_v3Input && !m_v1Input;
}

void CTextInput::enter(SP<CWLSurfaceResource> pSurface) {
    if (!pSurface || !pSurface->m_mapped)
        return;

    if (pSurface == focusedSurface())
        return;

    if (focusedSurface())
        leave();

    m_enterLocks++;
    if (m_enterLocks != 1) {
        Debug::log(ERR, "BUG THIS: TextInput has != 1 locks in enter");
        leave();
        m_enterLocks = 1;
    }

    if (isV3())
        m_v3Input->enter(pSurface);
    else {
        m_v1Input->enter(pSurface);
    }

    setFocusedSurface(pSurface);
}

void CTextInput::leave() {
    if (!focusedSurface())
        return;

    m_enterLocks--;
    if (m_enterLocks != 0) {
        Debug::log(ERR, "BUG THIS: TextInput has != 0 locks in leave");
        m_enterLocks = 0;
    }

    if (isV3())
        m_v3Input->leave(focusedSurface());
    else
        m_v1Input->leave();

    setFocusedSurface(nullptr);

    g_pInputManager->m_relay.deactivateIME(this);
}

SP<CWLSurfaceResource> CTextInput::focusedSurface() {
    return m_focusedSurface.lock();
}

wl_client* CTextInput::client() {
    return isV3() ? m_v3Input->client() : m_v1Input->client();
}

void CTextInput::commitStateToIME(SP<CInputMethodV2> ime) {
    if (isV3() && !m_v3Input.expired()) {
        const auto INPUT = m_v3Input.lock();

        if (INPUT->current.surrounding.updated)
            ime->surroundingText(INPUT->current.surrounding.text, INPUT->current.surrounding.cursor, INPUT->current.surrounding.anchor);

        ime->textChangeCause(INPUT->current.cause);

        if (INPUT->current.contentType.updated)
            ime->textContentType(INPUT->current.contentType.hint, INPUT->current.contentType.purpose);
    } else if (!m_v1Input.expired()) {
        const auto INPUT = m_v1Input.lock();

        if (INPUT->pendingSurrounding.isPending)
            ime->surroundingText(INPUT->pendingSurrounding.text, INPUT->pendingSurrounding.cursor, INPUT->pendingSurrounding.anchor);

        ime->textChangeCause(ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);

        if (m_v1Input->pendingContentType.isPending)
            ime->textContentType((zwpTextInputV3ContentHint)INPUT->pendingContentType.hint, (zwpTextInputV3ContentPurpose)INPUT->pendingContentType.purpose);
    }

    g_pInputManager->m_relay.updateAllPopups();

    ime->done();
}

void CTextInput::updateIMEState(SP<CInputMethodV2> ime) {
    if (isV3()) {
        const auto INPUT = m_v3Input.lock();

        if (ime->current.preeditString.committed)
            INPUT->preeditString(ime->current.preeditString.string, ime->current.preeditString.begin, ime->current.preeditString.end);

        if (ime->current.committedString.committed)
            INPUT->commitString(ime->current.committedString.string);

        if (ime->current.deleteSurrounding.committed)
            INPUT->deleteSurroundingText(ime->current.deleteSurrounding.before, ime->current.deleteSurrounding.after);

        INPUT->sendDone();
    } else {
        const auto INPUT = m_v1Input.lock();

        if (ime->current.preeditString.committed) {
            INPUT->preeditCursor(ime->current.preeditString.begin);
            INPUT->preeditStyling(0, std::string(ime->current.preeditString.string).length(), ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
            INPUT->preeditString(m_v1Input->serial, ime->current.preeditString.string.c_str(), "");
        } else {
            INPUT->preeditCursor(0);
            INPUT->preeditStyling(0, 0, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT);
            INPUT->preeditString(m_v1Input->serial, "", "");
        }

        if (ime->current.committedString.committed)
            INPUT->commitString(m_v1Input->serial, ime->current.committedString.string.c_str());

        if (ime->current.deleteSurrounding.committed) {
            INPUT->deleteSurroundingText(std::string(ime->current.preeditString.string).length() - ime->current.deleteSurrounding.before,
                                         ime->current.deleteSurrounding.after + ime->current.deleteSurrounding.before);

            if (ime->current.preeditString.committed)
                INPUT->commitString(m_v1Input->serial, ime->current.preeditString.string.c_str());
        }
    }
}

bool CTextInput::hasCursorRectangle() {
    return !isV3() || m_v3Input->current.box.updated;
}

CBox CTextInput::cursorBox() {
    return CBox{isV3() ? m_v3Input->current.box.cursorBox : m_v1Input->cursorRectangle};
}
