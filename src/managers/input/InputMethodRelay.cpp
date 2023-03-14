#include "InputMethodRelay.hpp"
#include "InputManager.hpp"
#include "../../Compositor.hpp"

CInputMethodRelay::CInputMethodRelay() {
    g_pHookSystem->hookDynamic("keyboardFocus", [&](void* self, std::any param) { onKeyboardFocus(std::any_cast<wlr_surface*>(param)); });
}

void CInputMethodRelay::onNewIME(wlr_input_method_v2* pIME) {
    if (m_pWLRIME) {
        Debug::log(ERR, "Cannot register 2 IMEs at once!");

        wlr_input_method_v2_send_unavailable(pIME);

        return;
    }

    m_pWLRIME = pIME;

    hyprListener_IMECommit.initCallback(
        &m_pWLRIME->events.commit,
        [&](void* owner, void* data) {
            const auto PTI  = getFocusedTextInput();
            const auto PIMR = (CInputMethodRelay*)owner;

            if (!PTI) {
                Debug::log(LOG, "No focused TextInput on IME Commit");
                return;
            }

            if (PTI->pWlrInput) {
                if (PIMR->m_pWLRIME->current.preedit.text) {
                    wlr_text_input_v3_send_preedit_string(PTI->pWlrInput, PIMR->m_pWLRIME->current.preedit.text, PIMR->m_pWLRIME->current.preedit.cursor_begin,
                                                          PIMR->m_pWLRIME->current.preedit.cursor_end);
                }

                if (PIMR->m_pWLRIME->current.commit_text) {
                    wlr_text_input_v3_send_commit_string(PTI->pWlrInput, PIMR->m_pWLRIME->current.commit_text);
                }

                if (PIMR->m_pWLRIME->current.delete_.before_length || PIMR->m_pWLRIME->current.delete_.after_length) {
                    wlr_text_input_v3_send_delete_surrounding_text(PTI->pWlrInput, PIMR->m_pWLRIME->current.delete_.before_length, PIMR->m_pWLRIME->current.delete_.after_length);
                }

                wlr_text_input_v3_send_done(PTI->pWlrInput);
            } else {
                if (PIMR->m_pWLRIME->current.preedit.text) {
                    zwp_text_input_v1_send_preedit_cursor(PTI->pV1Input->resourceImpl, PIMR->m_pWLRIME->current.preedit.cursor_begin);
                    zwp_text_input_v1_send_preedit_styling(PTI->pV1Input->resourceImpl, 0, std::string(PIMR->m_pWLRIME->current.preedit.text).length() - 1,
                                                           ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE);
                    zwp_text_input_v1_send_preedit_string(PTI->pV1Input->resourceImpl, PTI->pV1Input->serial, PIMR->m_pWLRIME->current.preedit.text, "");
                } else {
                    zwp_text_input_v1_send_preedit_cursor(PTI->pV1Input->resourceImpl, PIMR->m_pWLRIME->current.preedit.cursor_begin);
                    zwp_text_input_v1_send_preedit_styling(PTI->pV1Input->resourceImpl, 0, 0, ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE);
                    zwp_text_input_v1_send_preedit_string(PTI->pV1Input->resourceImpl, PTI->pV1Input->serial, "", "");
                }

                if (PIMR->m_pWLRIME->current.commit_text) {
                    zwp_text_input_v1_send_commit_string(PTI->pV1Input->resourceImpl, PTI->pV1Input->serial, PIMR->m_pWLRIME->current.commit_text);
                }

                if (PIMR->m_pWLRIME->current.delete_.before_length || PIMR->m_pWLRIME->current.delete_.after_length) {
                    zwp_text_input_v1_send_delete_surrounding_text(PTI->pV1Input->resourceImpl,
                                                                   std::string(PIMR->m_pWLRIME->current.preedit.text).length() - PIMR->m_pWLRIME->current.delete_.before_length,
                                                                   PIMR->m_pWLRIME->current.delete_.after_length + PIMR->m_pWLRIME->current.delete_.before_length);

                    if (PIMR->m_pWLRIME->current.preedit.text)
                        zwp_text_input_v1_send_commit_string(PTI->pV1Input->resourceImpl, PTI->pV1Input->serial, PIMR->m_pWLRIME->current.preedit.text);
                }
            }
        },
        this, "IMERelay");

    hyprListener_IMEDestroy.initCallback(
        &m_pWLRIME->events.destroy,
        [&](void* owner, void* data) {
            m_pWLRIME = nullptr;

            hyprListener_IMEDestroy.removeCallback();
            hyprListener_IMECommit.removeCallback();
            hyprListener_IMEGrab.removeCallback();
            hyprListener_IMENewPopup.removeCallback();

            m_pKeyboardGrab.reset(nullptr);

            const auto PTI = getFocusedTextInput();

            Debug::log(LOG, "IME Destroy");

            if (PTI) {
                setPendingSurface(PTI, focusedSurface(PTI));

                if (PTI->pWlrInput)
                    wlr_text_input_v3_send_leave(PTI->pWlrInput);
                else
                    zwp_text_input_v1_send_leave(PTI->pV1Input->resourceImpl);
            }
        },
        this, "IMERelay");

    hyprListener_IMEGrab.initCallback(
        &m_pWLRIME->events.grab_keyboard,
        [&](void* owner, void* data) {
            Debug::log(LOG, "IME TextInput Keyboard Grab new");

            m_pKeyboardGrab.reset(nullptr);

            m_pKeyboardGrab = std::make_unique<SIMEKbGrab>();

            m_pKeyboardGrab->pKeyboard = wlr_seat_get_keyboard(g_pCompositor->m_sSeat.seat);

            const auto PKBGRAB = (wlr_input_method_keyboard_grab_v2*)data;

            m_pKeyboardGrab->pWlrKbGrab = PKBGRAB;

            wlr_input_method_keyboard_grab_v2_set_keyboard(m_pKeyboardGrab->pWlrKbGrab, m_pKeyboardGrab->pKeyboard);

            m_pKeyboardGrab->hyprListener_grabDestroy.initCallback(
                &PKBGRAB->events.destroy,
                [&](void* owner, void* data) {
                    m_pKeyboardGrab->hyprListener_grabDestroy.removeCallback();

                    Debug::log(LOG, "IME TextInput Keyboard Grab destroy");

                    if (m_pKeyboardGrab->pKeyboard) {
                        wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sSeat.seat, &m_pKeyboardGrab->pKeyboard->modifiers);
                    }

                    m_pKeyboardGrab.reset(nullptr);
                },
                m_pKeyboardGrab.get(), "IME Keyboard Grab");
        },
        this, "IMERelay");

    hyprListener_IMENewPopup.initCallback(
        &m_pWLRIME->events.new_popup_surface,
        [&](void* owner, void* data) {
            const auto PNEWPOPUP = &m_lIMEPopups.emplace_back();

            PNEWPOPUP->pSurface = (wlr_input_popup_surface_v2*)data;

            PNEWPOPUP->hyprListener_commitPopup.initCallback(&PNEWPOPUP->pSurface->surface->events.commit, &Events::listener_commitInputPopup, PNEWPOPUP, "IME Popup");
            PNEWPOPUP->hyprListener_mapPopup.initCallback(&PNEWPOPUP->pSurface->events.map, &Events::listener_mapInputPopup, PNEWPOPUP, "IME Popup");
            PNEWPOPUP->hyprListener_unmapPopup.initCallback(&PNEWPOPUP->pSurface->events.unmap, &Events::listener_unmapInputPopup, PNEWPOPUP, "IME Popup");
            PNEWPOPUP->hyprListener_destroyPopup.initCallback(&PNEWPOPUP->pSurface->events.destroy, &Events::listener_destroyInputPopup, PNEWPOPUP, "IME Popup");

            Debug::log(LOG, "New input popup");
        },
        this, "IMERelay");

    const auto PTI = getFocusableTextInput();

    if (PTI) {
        if (PTI->pWlrInput)
            wlr_text_input_v3_send_enter(PTI->pWlrInput, PTI->pPendingSurface);
        else
            zwp_text_input_v1_send_enter(PTI->pV1Input->resourceImpl, PTI->pPendingSurface->resource);

        setPendingSurface(PTI, nullptr);
    }
}

wlr_surface* CInputMethodRelay::focusedSurface(STextInput* pTI) {
    return pTI->pWlrInput ? pTI->pWlrInput->focused_surface : pTI->pV1Input->focusedSurface;
}

void CInputMethodRelay::updateInputPopup(SIMEPopup* pPopup) {
    if (!pPopup->pSurface->mapped)
        return;

    // damage last known pos & size
    g_pHyprRenderer->damageBox(pPopup->realX, pPopup->realY, pPopup->lastSize.x, pPopup->lastSize.y);

    const auto PFOCUSEDTI = getFocusedTextInput();

    if (!PFOCUSEDTI || !focusedSurface(PFOCUSEDTI))
        return;

    bool       cursorRect      = PFOCUSEDTI->pWlrInput ? PFOCUSEDTI->pWlrInput->current.features & WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE : true;
    const auto PFOCUSEDSURFACE = focusedSurface(PFOCUSEDTI);
    auto       cursorBox       = PFOCUSEDTI->pWlrInput ? PFOCUSEDTI->pWlrInput->current.cursor_rectangle : PFOCUSEDTI->pV1Input->cursorRectangle;
    CMonitor*  pMonitor        = nullptr;

    Vector2D   parentPos;
    Vector2D   parentSize;

    if (wlr_layer_surface_v1_try_from_wlr_surface(PFOCUSEDSURFACE)) {
        const auto PLS = g_pCompositor->getLayerSurfaceFromWlr(wlr_layer_surface_v1_try_from_wlr_surface(PFOCUSEDSURFACE));

        if (PLS) {
            parentPos  = Vector2D(PLS->geometry.x, PLS->geometry.y) + g_pCompositor->getMonitorFromID(PLS->monitorID)->vecPosition;
            parentSize = Vector2D(PLS->geometry.width, PLS->geometry.height);
            pMonitor   = g_pCompositor->getMonitorFromID(PLS->monitorID);
        }
    } else {
        const auto PWINDOW = g_pCompositor->getWindowFromSurface(PFOCUSEDSURFACE);

        if (PWINDOW) {
            parentPos  = PWINDOW->m_vRealPosition.goalv();
            parentSize = PWINDOW->m_vRealSize.goalv();
            pMonitor   = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
        }
    }

    if (!cursorRect) {
        cursorBox = {0, 0, (int)parentSize.x, (int)parentSize.y};
    }

    if (!pMonitor)
        return;

    wlr_box finalBox = cursorBox;

    if (cursorBox.y + parentPos.y + pPopup->pSurface->surface->current.height + finalBox.height > pMonitor->vecPosition.y + pMonitor->vecSize.y)
        finalBox.y -= pPopup->pSurface->surface->current.height + finalBox.height;

    if (cursorBox.x + parentPos.x + pPopup->pSurface->surface->current.width > pMonitor->vecPosition.x + pMonitor->vecSize.x)
        finalBox.x -= (cursorBox.x + parentPos.x + pPopup->pSurface->surface->current.width) - (pMonitor->vecPosition.x + pMonitor->vecSize.x);

    pPopup->x = finalBox.x;
    pPopup->y = finalBox.y + finalBox.height;

    pPopup->realX = finalBox.x + parentPos.x;
    pPopup->realY = finalBox.y + parentPos.y + finalBox.height;

    pPopup->lastSize = Vector2D(pPopup->pSurface->surface->current.width, pPopup->pSurface->surface->current.height);

    wlr_input_popup_surface_v2_send_text_input_rectangle(pPopup->pSurface, &finalBox);

    damagePopup(pPopup);
}

void CInputMethodRelay::setIMEPopupFocus(SIMEPopup* pPopup, wlr_surface* pSurface) {
    updateInputPopup(pPopup);
}

void Events::listener_mapInputPopup(void* owner, void* data) {
    const auto PPOPUP = (SIMEPopup*)owner;

    Debug::log(LOG, "Mapped an IME Popup");

    g_pInputManager->m_sIMERelay.updateInputPopup(PPOPUP);

    if (const auto PMONITOR = g_pCompositor->getMonitorFromVector(Vector2D(PPOPUP->realX, PPOPUP->realY) + PPOPUP->lastSize / 2.f); PMONITOR)
        wlr_surface_send_enter(PPOPUP->pSurface->surface, PMONITOR->output);
}

void Events::listener_unmapInputPopup(void* owner, void* data) {
    const auto PPOPUP = (SIMEPopup*)owner;

    Debug::log(LOG, "Unmapped an IME Popup");

    g_pHyprRenderer->damageBox(PPOPUP->realX, PPOPUP->realY, PPOPUP->lastSize.x, PPOPUP->lastSize.y);

    g_pInputManager->m_sIMERelay.updateInputPopup(PPOPUP);
}

void Events::listener_destroyInputPopup(void* owner, void* data) {
    const auto PPOPUP = (SIMEPopup*)owner;

    Debug::log(LOG, "Removed an IME Popup");

    PPOPUP->hyprListener_commitPopup.removeCallback();
    PPOPUP->hyprListener_destroyPopup.removeCallback();
    PPOPUP->hyprListener_focusedSurfaceUnmap.removeCallback();
    PPOPUP->hyprListener_mapPopup.removeCallback();
    PPOPUP->hyprListener_unmapPopup.removeCallback();

    g_pInputManager->m_sIMERelay.removePopup(PPOPUP);
}

void Events::listener_commitInputPopup(void* owner, void* data) {
    const auto PPOPUP = (SIMEPopup*)owner;

    g_pInputManager->m_sIMERelay.updateInputPopup(PPOPUP);
}

void CInputMethodRelay::removePopup(SIMEPopup* pPopup) {
    m_lIMEPopups.remove(*pPopup);
}

void CInputMethodRelay::damagePopup(SIMEPopup* pPopup) {
    if (!pPopup->pSurface->mapped)
        return;

    const auto PFOCUSEDTI = getFocusedTextInput();

    if (!PFOCUSEDTI || !focusedSurface(PFOCUSEDTI))
        return;

    Vector2D   parentPos;

    const auto PFOCUSEDSURFACE = focusedSurface(PFOCUSEDTI);

    if (wlr_layer_surface_v1_try_from_wlr_surface(PFOCUSEDSURFACE)) {
        const auto PLS = g_pCompositor->getLayerSurfaceFromWlr(wlr_layer_surface_v1_try_from_wlr_surface(PFOCUSEDSURFACE));

        if (PLS) {
            parentPos = Vector2D(PLS->geometry.x, PLS->geometry.y) + g_pCompositor->getMonitorFromID(PLS->monitorID)->vecPosition;
        }
    } else {
        const auto PWINDOW = g_pCompositor->getWindowFromSurface(PFOCUSEDSURFACE);

        if (PWINDOW) {
            parentPos = PWINDOW->m_vRealPosition.goalv();
        }
    }

    g_pHyprRenderer->damageSurface(pPopup->pSurface->surface, parentPos.x + pPopup->x, parentPos.y + pPopup->y);
}

SIMEKbGrab* CInputMethodRelay::getIMEKeyboardGrab(SKeyboard* pKeyboard) {

    if (!m_pWLRIME)
        return nullptr;

    if (!m_pKeyboardGrab.get())
        return nullptr;

    const auto VIRTKB = wlr_input_device_get_virtual_keyboard(pKeyboard->keyboard);

    if (VIRTKB && (wl_resource_get_client(VIRTKB->resource) == wl_resource_get_client(m_pKeyboardGrab->pWlrKbGrab->resource)))
        return nullptr;

    return m_pKeyboardGrab.get();
}

STextInput* CInputMethodRelay::getFocusedTextInput() {

    for (auto& ti : m_lTextInputs) {
        if (focusedSurface(&ti)) {
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

void CInputMethodRelay::createNewTextInput(wlr_text_input_v3* pInput, STextInputV1* pTIV1) {
    const auto PTEXTINPUT = &m_lTextInputs.emplace_back();

    PTEXTINPUT->pWlrInput = pInput;
    PTEXTINPUT->pV1Input  = pTIV1;

    if (pTIV1)
        pTIV1->pTextInput = PTEXTINPUT;

    PTEXTINPUT->hyprListener_textInputEnable.initCallback(
        pInput ? &pInput->events.enable : &pTIV1->sEnable,
        [](void* owner, void* data) {
            const auto PINPUT = (STextInput*)owner;

            if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
                // Debug::log(WARN, "Enabling TextInput on no IME!");
                return;
            }

            Debug::log(LOG, "Enable TextInput");

            wlr_input_method_v2_send_activate(g_pInputManager->m_sIMERelay.m_pWLRIME);
            g_pInputManager->m_sIMERelay.commitIMEState(PINPUT);
        },
        PTEXTINPUT, "textInput");

    PTEXTINPUT->hyprListener_textInputCommit.initCallback(
        pInput ? &pInput->events.commit : &pTIV1->sCommit,
        [](void* owner, void* data) {
            const auto PINPUT = (STextInput*)owner;

            if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
                //   Debug::log(WARN, "Committing TextInput on no IME!");
                return;
            }

            if (!(PINPUT->pWlrInput ? PINPUT->pWlrInput->current_enabled : PINPUT->pV1Input->active)) {
                Debug::log(WARN, "Disabled TextInput commit?");
                return;
            }

            g_pInputManager->m_sIMERelay.commitIMEState(PINPUT);
        },
        PTEXTINPUT, "textInput");

    PTEXTINPUT->hyprListener_textInputDisable.initCallback(
        pInput ? &pInput->events.disable : &pTIV1->sDisable,
        [](void* owner, void* data) {
            const auto PINPUT = (STextInput*)owner;

            if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
                //  Debug::log(WARN, "Disabling TextInput on no IME!");
                return;
            }

            Debug::log(LOG, "Disable TextInput");

            wlr_input_method_v2_send_deactivate(g_pInputManager->m_sIMERelay.m_pWLRIME);

            g_pInputManager->m_sIMERelay.commitIMEState(PINPUT);
        },
        PTEXTINPUT, "textInput");

    PTEXTINPUT->hyprListener_textInputDestroy.initCallback(
        pInput ? &pInput->events.destroy : &pTIV1->sDestroy,
        [](void* owner, void* data) {
            const auto PINPUT = (STextInput*)owner;

            if (!g_pInputManager->m_sIMERelay.m_pWLRIME) {
                //  Debug::log(WARN, "Disabling TextInput on no IME!");
                return;
            }

            if (PINPUT->pWlrInput && PINPUT->pWlrInput->current_enabled) {
                wlr_input_method_v2_send_deactivate(g_pInputManager->m_sIMERelay.m_pWLRIME);

                g_pInputManager->m_sIMERelay.commitIMEState(PINPUT);
            }

            g_pInputManager->m_sIMERelay.setPendingSurface(PINPUT, nullptr);

            PINPUT->hyprListener_textInputCommit.removeCallback();
            PINPUT->hyprListener_textInputDestroy.removeCallback();
            PINPUT->hyprListener_textInputDisable.removeCallback();
            PINPUT->hyprListener_textInputEnable.removeCallback();

            g_pInputManager->m_sIMERelay.removeTextInput(PINPUT);
        },
        PTEXTINPUT, "textInput");
}

void CInputMethodRelay::removeTextInput(STextInput* pInput) {
    m_lTextInputs.remove_if([&](const auto& other) { return other.pWlrInput == pInput->pWlrInput && other.pV1Input == pInput->pV1Input; });
}

void CInputMethodRelay::commitIMEState(STextInput* pInput) {
    if (!m_pWLRIME)
        return;

    if (pInput->pWlrInput) {
        // V3
        if (pInput->pWlrInput->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT)
            wlr_input_method_v2_send_surrounding_text(m_pWLRIME, pInput->pWlrInput->current.surrounding.text, pInput->pWlrInput->current.surrounding.cursor,
                                                      pInput->pWlrInput->current.surrounding.anchor);

        wlr_input_method_v2_send_text_change_cause(m_pWLRIME, pInput->pWlrInput->current.text_change_cause);

        if (pInput->pWlrInput->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE)
            wlr_input_method_v2_send_content_type(m_pWLRIME, pInput->pWlrInput->current.content_type.hint, pInput->pWlrInput->current.content_type.purpose);
    } else {
        // V1
        if (pInput->pV1Input->pendingSurrounding.isPending)
            wlr_input_method_v2_send_surrounding_text(m_pWLRIME, pInput->pV1Input->pendingSurrounding.text.c_str(), pInput->pV1Input->pendingSurrounding.cursor,
                                                      pInput->pV1Input->pendingSurrounding.anchor);

        wlr_input_method_v2_send_text_change_cause(m_pWLRIME, 0);

        if (pInput->pV1Input->pendingContentType.isPending)
            wlr_input_method_v2_send_content_type(m_pWLRIME, pInput->pV1Input->pendingContentType.hint, pInput->pV1Input->pendingContentType.purpose);
    }

    for (auto& p : m_lIMEPopups) {
        updateInputPopup(&p);
    }

    wlr_input_method_v2_send_done(m_pWLRIME);
}

void CInputMethodRelay::onKeyboardFocus(wlr_surface* pSurface) {
    if (!m_pWLRIME)
        return;

    auto client = [](STextInput* pTI) -> wl_client* { return pTI->pWlrInput ? wl_resource_get_client(pTI->pWlrInput->resource) : pTI->pV1Input->client; };

    for (auto& ti : m_lTextInputs) {
        if (ti.pPendingSurface) {

            if (pSurface != ti.pPendingSurface)
                setPendingSurface(&ti, nullptr);

        } else if (focusedSurface(&ti)) {

            if (pSurface != focusedSurface(&ti)) {
                wlr_input_method_v2_send_deactivate(m_pWLRIME);
                commitIMEState(&ti);

                if (ti.pWlrInput)
                    wlr_text_input_v3_send_leave(ti.pWlrInput);
                else {
                    zwp_text_input_v1_send_leave(ti.pV1Input->resourceImpl);
                    ti.pV1Input->focusedSurface = nullptr;
                    ti.pV1Input->active         = false;
                }
            } else {
                continue;
            }
        }

        if (pSurface && client(&ti) == wl_resource_get_client(pSurface->resource)) {

            if (m_pWLRIME) {
                if (ti.pWlrInput)
                    wlr_text_input_v3_send_enter(ti.pWlrInput, pSurface);
                else {
                    zwp_text_input_v1_send_enter(ti.pV1Input->resourceImpl, pSurface->resource);
                    ti.pV1Input->focusedSurface = pSurface;
                    ti.pV1Input->active         = true;
                }
            } else {
                setPendingSurface(&ti, pSurface);
            }
        }
    }
}

void CInputMethodRelay::setPendingSurface(STextInput* pInput, wlr_surface* pSurface) {
    pInput->pPendingSurface = pSurface;

    if (pSurface) {
        pInput->hyprListener_pendingSurfaceDestroy.initCallback(
            &pSurface->events.destroy,
            [](void* owner, void* data) {
                const auto PINPUT = (STextInput*)owner;

                PINPUT->pPendingSurface = nullptr;

                PINPUT->hyprListener_pendingSurfaceDestroy.removeCallback();
            },
            pInput, "TextInput");
    } else {
        pInput->hyprListener_pendingSurfaceDestroy.removeCallback();
    }
}
