#include "InputMethodRelay.hpp"
#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/TextInputV3.hpp"
#include "../../protocols/InputMethodV2.hpp"

CInputMethodRelay::CInputMethodRelay() {
    static auto P = g_pHookSystem->hookDynamic("keyboardFocus", [&](void* self, SCallbackInfo& info, std::any param) { onKeyboardFocus(std::any_cast<wlr_surface*>(param)); });

    listeners.newTIV3 = PROTO::textInputV3->events.newTextInput.registerListener([this](std::any ti) { onNewTextInput(ti); });
    listeners.newIME  = PROTO::ime->events.newIME.registerListener([this](std::any ime) { onNewIME(std::any_cast<SP<CInputMethodV2>>(ime)); });
}

void CInputMethodRelay::onNewIME(SP<CInputMethodV2> pIME) {
    if (!m_pIME.expired()) {
        Debug::log(ERR, "Cannot register 2 IMEs at once!");

        pIME->unavailable();

        return;
    }

    m_pIME = pIME;

    listeners.commitIME = pIME->events.onCommit.registerListener([this](std::any d) {
        const auto PTI = getFocusedTextInput();

        if (!PTI) {
            Debug::log(LOG, "No focused TextInput on IME Commit");
            return;
        }

        PTI->updateIMEState(m_pIME.lock());
    });

    listeners.destroyIME = pIME->events.destroy.registerListener([this](std::any d) {
        const auto PTI = getFocusedTextInput();

        Debug::log(LOG, "IME Destroy");

        if (PTI)
            PTI->leave();

        m_pIME.reset();
    });

    listeners.newPopup = pIME->events.newPopup.registerListener([this](std::any d) {
        m_vIMEPopups.emplace_back(std::make_unique<CInputPopup>(std::any_cast<SP<CInputMethodPopupV2>>(d)));

        Debug::log(LOG, "New input popup");
    });

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

CTextInput* CInputMethodRelay::getFocusedTextInput() {
    if (!g_pCompositor->m_pLastFocus)
        return nullptr;

    for (auto& ti : m_vTextInputs) {
        if (ti->focusedSurface() == g_pCompositor->m_pLastFocus)
            return ti.get();
    }

    return nullptr;
}

void CInputMethodRelay::onNewTextInput(std::any tiv3) {
    m_vTextInputs.emplace_back(std::make_unique<CTextInput>(std::any_cast<WP<CTextInputV3>>(tiv3)));
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
    if (m_pIME.expired())
        return;

    m_pIME->activate();
    commitIMEState(pInput);
}

void CInputMethodRelay::deactivateIME(CTextInput* pInput) {
    if (m_pIME.expired())
        return;

    m_pIME->deactivate();
    commitIMEState(pInput);
}

void CInputMethodRelay::commitIMEState(CTextInput* pInput) {
    if (m_pIME.expired())
        return;

    pInput->commitStateToIME(m_pIME.lock());
}

void CInputMethodRelay::onKeyboardFocus(wlr_surface* pSurface) {
    if (m_pIME.expired())
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
