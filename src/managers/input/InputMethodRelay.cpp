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

    m_listeners.newTIV3 = PROTO::textInputV3->events.newTextInput.registerListener([this](std::any ti) { onNewTextInput(std::any_cast<WP<CTextInputV3>>(ti)); });
    m_listeners.newTIV1 = PROTO::textInputV1->events.newTextInput.registerListener([this](std::any ti) { onNewTextInput(std::any_cast<WP<CTextInputV1>>(ti)); });
    m_listeners.newIME  = PROTO::ime->events.newIME.registerListener([this](std::any ime) { onNewIME(std::any_cast<SP<CInputMethodV2>>(ime)); });
}

void CInputMethodRelay::onNewIME(SP<CInputMethodV2> pIME) {
    if (!m_pIME.expired()) {
        NDebug::log(ERR, "Cannot register 2 IMEs at once!");

        pIME->unavailable();

        return;
    }

    m_pIME = pIME;

    m_listeners.commitIME = pIME->events.onCommit.registerListener([this](std::any d) {
        const auto PTI = getFocusedTextInput();

        if (!PTI) {
            NDebug::log(LOG, "No focused TextInput on IME Commit");
            return;
        }

        PTI->updateIMEState(m_pIME.lock());
    });

    m_listeners.destroyIME = pIME->events.destroy.registerListener([this](std::any d) {
        const auto PTI = getFocusedTextInput();

        NDebug::log(LOG, "IME Destroy");

        if (PTI)
            PTI->leave();

        m_pIME.reset();
    });

    m_listeners.newPopup = pIME->events.newPopup.registerListener([this](std::any d) {
        m_vIMEPopups.emplace_back(makeUnique<CInputPopup>(std::any_cast<SP<CInputMethodPopupV2>>(d)));

        NDebug::log(LOG, "New input popup");
    });

    if (!g_pCompositor->m_pLastFocus)
        return;

    for (auto const& ti : m_vTextInputs) {
        if (ti->client() != g_pCompositor->m_pLastFocus->client())
            continue;

        if (ti->isV3())
            ti->enter(g_pCompositor->m_pLastFocus.lock());
        else
            ti->onEnabled(g_pCompositor->m_pLastFocus.lock());
    }
}

void CInputMethodRelay::removePopup(CInputPopup* pPopup) {
    std::erase_if(m_vIMEPopups, [pPopup](const auto& other) { return other.get() == pPopup; });
}

CTextInput* CInputMethodRelay::getFocusedTextInput() {
    if (!g_pCompositor->m_pLastFocus)
        return nullptr;

    for (auto const& ti : m_vTextInputs) {
        if (ti->focusedSurface() == g_pCompositor->m_pLastFocus)
            return ti.get();
    }

    return nullptr;
}

void CInputMethodRelay::onNewTextInput(WP<CTextInputV3> tiv3) {
    m_vTextInputs.emplace_back(makeUnique<CTextInput>(tiv3));
}

void CInputMethodRelay::onNewTextInput(WP<CTextInputV1> pTIV1) {
    m_vTextInputs.emplace_back(makeUnique<CTextInput>(pTIV1));
}

void CInputMethodRelay::removeTextInput(CTextInput* pInput) {
    std::erase_if(m_vTextInputs, [pInput](const auto& other) { return other.get() == pInput; });
}

void CInputMethodRelay::updateAllPopups() {
    for (auto const& p : m_vIMEPopups) {
        p->onCommit();
    }
}

void CInputMethodRelay::activateIME(CTextInput* pInput, bool shouldCommit) {
    if (m_pIME.expired())
        return;

    m_pIME->activate();
    if (shouldCommit)
        commitIMEState(pInput);
}

void CInputMethodRelay::deactivateIME(CTextInput* pInput, bool shouldCommit) {
    if (m_pIME.expired())
        return;

    m_pIME->deactivate();
    if (shouldCommit)
        commitIMEState(pInput);
}

void CInputMethodRelay::commitIMEState(CTextInput* pInput) {
    if (m_pIME.expired())
        return;

    pInput->commitStateToIME(m_pIME.lock());
}

void CInputMethodRelay::onKeyboardFocus(SP<CWLSurfaceResource> pSurface) {
    if (m_pIME.expired())
        return;

    if (pSurface == m_pLastKbFocus)
        return;

    m_pLastKbFocus = pSurface;

    for (auto const& ti : m_vTextInputs) {
        if (!ti->focusedSurface())
            continue;

        ti->leave();
    }

    if (!pSurface)
        return;

    for (auto const& ti : m_vTextInputs) {
        if (!ti->isV3())
            continue;

        if (ti->client() != pSurface->client())
            continue;

        ti->enter(pSurface);
    }
}

CInputPopup* CInputMethodRelay::popupFromCoords(const Vector2D& point) {
    for (auto const& p : m_vIMEPopups) {
        if (p->isVecInPopup(point))
            return p.get();
    }

    return nullptr;
}

CInputPopup* CInputMethodRelay::popupFromSurface(const SP<CWLSurfaceResource> surface) {
    for (auto const& p : m_vIMEPopups) {
        if (p->getSurface() == surface)
            return p.get();
    }

    return nullptr;
}
