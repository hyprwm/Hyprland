#include "InputMethodRelay.hpp"
#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/TextInputV3.hpp"
#include "../../protocols/TextInputV1.hpp"
#include "../../protocols/InputMethodV2.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../managers/HookSystemManager.hpp"

CInputMethodRelay::CInputMethodRelay() {
    static auto P =
        g_pHookSystem->hookDynamic("keyboardFocus", [&](void* self, SCallbackInfo& info, std::any param) { onKeyboardFocus(std::any_cast<SP<CWLSurfaceResource>>(param)); });

    m_listeners.newTIV3 = PROTO::textInputV3->m_events.newTextInput.listen([this](const auto& input) { onNewTextInput(input); });
    m_listeners.newTIV1 = PROTO::textInputV1->m_events.newTextInput.listen([this](const auto& input) { onNewTextInput(input); });
    m_listeners.newIME  = PROTO::ime->m_events.newIME.listen([this](const auto& ime) { onNewIME(ime); });
}

void CInputMethodRelay::onNewIME(SP<CInputMethodV2> pIME) {
    if (!m_inputMethod.expired()) {
        Debug::log(ERR, "Cannot register 2 IMEs at once!");

        pIME->unavailable();

        return;
    }

    m_inputMethod = pIME;

    m_listeners.commitIME = pIME->m_events.onCommit.listen([this] {
        const auto PTI = getFocusedTextInput();

        if (!PTI) {
            Debug::log(LOG, "No focused TextInput on IME Commit");
            return;
        }

        PTI->updateIMEState(m_inputMethod.lock());
    });

    m_listeners.destroyIME = pIME->m_events.destroy.listen([this] {
        const auto PTI = getFocusedTextInput();

        Debug::log(LOG, "IME Destroy");

        if (PTI)
            PTI->leave();

        m_inputMethod.reset();
    });

    m_listeners.newPopup = pIME->m_events.newPopup.listen([this](const SP<CInputMethodPopupV2>& popup) {
        m_inputMethodPopups.emplace_back(makeUnique<CInputPopup>(popup));
        Debug::log(LOG, "New input popup");
    });

    if (!g_pCompositor->m_lastFocus)
        return;

    for (auto const& ti : m_textInputs) {
        if (ti->client() != g_pCompositor->m_lastFocus->client())
            continue;

        if (ti->isV3())
            ti->enter(g_pCompositor->m_lastFocus.lock());
        else
            ti->onEnabled(g_pCompositor->m_lastFocus.lock());
    }
}

void CInputMethodRelay::removePopup(CInputPopup* pPopup) {
    std::erase_if(m_inputMethodPopups, [pPopup](const auto& other) { return other.get() == pPopup; });
}

CTextInput* CInputMethodRelay::getFocusedTextInput() {
    if (!g_pCompositor->m_lastFocus)
        return nullptr;

    for (auto const& ti : m_textInputs) {
        if (ti->focusedSurface() == g_pCompositor->m_lastFocus)
            return ti.get();
    }

    return nullptr;
}

void CInputMethodRelay::onNewTextInput(WP<CTextInputV3> tiv3) {
    m_textInputs.emplace_back(makeUnique<CTextInput>(tiv3));
}

void CInputMethodRelay::onNewTextInput(WP<CTextInputV1> pTIV1) {
    m_textInputs.emplace_back(makeUnique<CTextInput>(pTIV1));
}

void CInputMethodRelay::removeTextInput(CTextInput* pInput) {
    std::erase_if(m_textInputs, [pInput](const auto& other) { return other.get() == pInput; });
}

void CInputMethodRelay::updateAllPopups() {
    for (auto const& p : m_inputMethodPopups) {
        p->onCommit();
    }
}

void CInputMethodRelay::activateIME(CTextInput* pInput, bool shouldCommit) {
    if (m_inputMethod.expired())
        return;

    m_inputMethod->activate();
    if (shouldCommit)
        commitIMEState(pInput);
}

void CInputMethodRelay::deactivateIME(CTextInput* pInput, bool shouldCommit) {
    if (m_inputMethod.expired())
        return;

    m_inputMethod->deactivate();
    if (shouldCommit)
        commitIMEState(pInput);
}

void CInputMethodRelay::commitIMEState(CTextInput* pInput) {
    if (m_inputMethod.expired())
        return;

    pInput->commitStateToIME(m_inputMethod.lock());
}

void CInputMethodRelay::onKeyboardFocus(SP<CWLSurfaceResource> pSurface) {
    if (m_inputMethod.expired())
        return;

    if (pSurface == m_lastKbFocus)
        return;

    m_lastKbFocus = pSurface;

    for (auto const& ti : m_textInputs) {
        if (!ti->focusedSurface())
            continue;

        ti->leave();
    }

    if (!pSurface)
        return;

    for (auto const& ti : m_textInputs) {
        if (!ti->isV3())
            continue;

        if (ti->client() != pSurface->client())
            continue;

        ti->enter(pSurface);
    }
}

CInputPopup* CInputMethodRelay::popupFromCoords(const Vector2D& point) {
    for (auto const& p : m_inputMethodPopups) {
        if (p->isVecInPopup(point))
            return p.get();
    }

    return nullptr;
}

CInputPopup* CInputMethodRelay::popupFromSurface(const SP<CWLSurfaceResource> surface) {
    for (auto const& p : m_inputMethodPopups) {
        if (p->getSurface() == surface)
            return p.get();
    }

    return nullptr;
}
