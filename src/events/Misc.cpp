#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"

// ------------------------------ //
//   __  __ _____  _____  _____   //
//  |  \/  |_   _|/ ____|/ ____|  //
//  | \  / | | | | (___ | |       //
//  | |\/| | | |  \___ \| |       //
//  | |  | |_| |_ ____) | |____   //
//  |_|  |_|_____|_____/ \_____|  //
//                                //
// ------------------------------ //

void Events::listener_outputMgrApply(wl_listener* listener, void* data) {
    const auto CONFIG = (wlr_output_configuration_v1*)data;
    g_pHyprRenderer->outputMgrApplyTest(CONFIG, false);
}

void Events::listener_outputMgrTest(wl_listener* listener, void* data) {
    const auto CONFIG = (wlr_output_configuration_v1*)data;
    g_pHyprRenderer->outputMgrApplyTest(CONFIG, true);
}

void Events::listener_leaseRequest(wl_listener* listener, void* data) {
    const auto               REQUEST = (wlr_drm_lease_request_v1*)data;
    struct wlr_drm_lease_v1* lease   = wlr_drm_lease_request_v1_grant(REQUEST);
    if (!lease) {
        Debug::log(ERR, "Failed to grant lease request!");
        wlr_drm_lease_request_v1_reject(REQUEST);
    }
}

void Events::listener_requestSetPrimarySel(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_request_set_primary_selection_event*)data;
    wlr_seat_set_primary_selection(g_pCompositor->m_sSeat.seat, EVENT->source, EVENT->serial);
}

void Events::listener_requestSetSel(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_request_set_selection_event*)data;
    wlr_seat_set_selection(g_pCompositor->m_sSeat.seat, EVENT->source, EVENT->serial);
}

void Events::listener_readyXWayland(wl_listener* listener, void* data) {
#ifndef NO_XWAYLAND
    const auto XCBCONNECTION = xcb_connect(g_pXWaylandManager->m_sWLRXWayland->display_name, NULL);
    const auto ERR           = xcb_connection_has_error(XCBCONNECTION);
    if (ERR) {
        Debug::log(LogLevel::ERR, "XWayland -> xcb_connection_has_error failed with {}", ERR);
        return;
    }

    for (auto& ATOM : HYPRATOMS) {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(XCBCONNECTION, 0, ATOM.first.length(), ATOM.first.c_str());
        xcb_intern_atom_reply_t* reply  = xcb_intern_atom_reply(XCBCONNECTION, cookie, NULL);

        if (!reply) {
            Debug::log(LogLevel::ERR, "XWayland -> Atom failed: {}", ATOM.first);
            continue;
        }

        ATOM.second = reply->atom;
    }

    wlr_xwayland_set_seat(g_pXWaylandManager->m_sWLRXWayland, g_pCompositor->m_sSeat.seat);

    const auto XCURSOR = wlr_xcursor_manager_get_xcursor(g_pCompositor->m_sWLRXCursorMgr, "left_ptr", 1);
    if (XCURSOR) {
        wlr_xwayland_set_cursor(g_pXWaylandManager->m_sWLRXWayland, XCURSOR->images[0]->buffer, XCURSOR->images[0]->width * 4, XCURSOR->images[0]->width,
                                XCURSOR->images[0]->height, XCURSOR->images[0]->hotspot_x, XCURSOR->images[0]->hotspot_y);
    }

    xcb_disconnect(XCBCONNECTION);
#endif
}

void Events::listener_requestDrag(wl_listener* listener, void* data) {
    const auto E = (wlr_seat_request_start_drag_event*)data;

    if (!wlr_seat_validate_pointer_grab_serial(g_pCompositor->m_sSeat.seat, E->origin, E->serial)) {
        Debug::log(LOG, "Ignoring drag and drop request: serial mismatch.");
        wlr_data_source_destroy(E->drag->source);
        return;
    }

    wlr_seat_start_pointer_drag(g_pCompositor->m_sSeat.seat, E->drag, E->serial);
}

void Events::listener_startDrag(wl_listener* listener, void* data) {

    if (g_pInputManager->m_sDrag.drag)
        return; // don't handle multiple drags

    g_pInputManager->m_sDrag.drag = (wlr_drag*)data;

    wlr_drag* wlrDrag = (wlr_drag*)data;

    Debug::log(LOG, "Started drag {:x}", (uintptr_t)wlrDrag);

    wlrDrag->data = data;

    g_pInputManager->m_sDrag.hyprListener_destroy.initCallback(&wlrDrag->events.destroy, &Events::listener_destroyDrag, &g_pInputManager->m_sDrag, "Drag");

    if (wlrDrag->icon) {
        Debug::log(LOG, "Drag started with an icon {:x}", (uintptr_t)wlrDrag->icon);

        g_pInputManager->m_sDrag.dragIcon = wlrDrag->icon;
        wlrDrag->icon->data               = g_pInputManager->m_sDrag.dragIcon;

        g_pInputManager->m_sDrag.hyprListener_mapIcon.initCallback(&wlrDrag->icon->surface->events.map, &Events::listener_mapDragIcon, &g_pInputManager->m_sDrag, "DragIcon");
        g_pInputManager->m_sDrag.hyprListener_unmapIcon.initCallback(&wlrDrag->icon->surface->events.unmap, &Events::listener_unmapDragIcon, &g_pInputManager->m_sDrag, "DragIcon");
        g_pInputManager->m_sDrag.hyprListener_destroyIcon.initCallback(&wlrDrag->icon->events.destroy, &Events::listener_destroyDragIcon, &g_pInputManager->m_sDrag, "DragIcon");
        g_pInputManager->m_sDrag.hyprListener_commitIcon.initCallback(&wlrDrag->icon->surface->events.commit, &Events::listener_commitDragIcon, &g_pInputManager->m_sDrag,
                                                                      "DragIcon");
    }
}

void Events::listener_destroyDrag(void* owner, void* data) {
    Debug::log(LOG, "Drag destroyed.");

    if (g_pInputManager->m_sDrag.drag && g_pInputManager->m_sDrag.dragIcon && g_pInputManager->m_sDrag.dragIcon->surface)
        g_pHyprRenderer->damageBox(g_pInputManager->m_sDrag.pos.x - 2, g_pInputManager->m_sDrag.pos.y - 2, g_pInputManager->m_sDrag.dragIcon->surface->current.width + 4,
                                   g_pInputManager->m_sDrag.dragIcon->surface->current.height + 4);

    g_pInputManager->m_sDrag.drag     = nullptr;
    g_pInputManager->m_sDrag.dragIcon = nullptr;
    g_pInputManager->m_sDrag.hyprListener_destroy.removeCallback();

    g_pCompositor->focusWindow(g_pCompositor->m_pLastWindow, g_pCompositor->m_pLastWindow ? g_pXWaylandManager->getWindowSurface(g_pCompositor->m_pLastWindow) : nullptr);
}

void Events::listener_mapDragIcon(void* owner, void* data) {
    Debug::log(LOG, "Drag icon mapped.");
    g_pInputManager->m_sDrag.iconMapped = true;
}

void Events::listener_unmapDragIcon(void* owner, void* data) {
    Debug::log(LOG, "Drag icon unmapped.");
    g_pInputManager->m_sDrag.iconMapped = false;
}

void Events::listener_destroyDragIcon(void* owner, void* data) {
    Debug::log(LOG, "Drag icon destroyed.");

    g_pInputManager->m_sDrag.dragIcon = nullptr;
    g_pInputManager->m_sDrag.hyprListener_commitIcon.removeCallback();
    g_pInputManager->m_sDrag.hyprListener_destroyIcon.removeCallback();
    g_pInputManager->m_sDrag.hyprListener_mapIcon.removeCallback();
    g_pInputManager->m_sDrag.hyprListener_unmapIcon.removeCallback();
}

void Events::listener_commitDragIcon(void* owner, void* data) {
    g_pInputManager->updateDragIcon();

    Debug::log(LOG, "Drag icon committed.");
}

void Events::listener_RendererDestroy(wl_listener* listener, void* data) {
    Debug::log(LOG, "!!Renderer destroyed!!");
}

void Events::listener_sessionActive(wl_listener* listener, void* data) {
    Debug::log(LOG, "Session got activated!");

    g_pCompositor->m_bSessionActive = true;

    for (auto& m : g_pCompositor->m_vMonitors) {
        g_pCompositor->scheduleFrameForMonitor(m.get());
    }

    g_pConfigManager->m_bWantsMonitorReload = true;
}

void Events::listener_powerMgrSetMode(wl_listener* listener, void* data) {
    Debug::log(LOG, "PowerMgr set mode!");

    const auto EVENT    = (wlr_output_power_v1_set_mode_event*)data;
    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(EVENT->output);

    if (!PMONITOR) {
        Debug::log(ERR, "Invalid powerMgrSetMode output");
        return;
    }

    wlr_output_state_set_enabled(PMONITOR->state.wlr(), EVENT->mode == 1);

    if (!PMONITOR->state.commit())
        Debug::log(ERR, "Couldn't set power mode");
}

void Events::listener_newIME(wl_listener* listener, void* data) {
    Debug::log(LOG, "New IME added!");

    g_pInputManager->m_sIMERelay.onNewIME((wlr_input_method_v2*)data);
}

void Events::listener_newTextInput(wl_listener* listener, void* data) {
    Debug::log(LOG, "New TextInput added!");

    g_pInputManager->m_sIMERelay.onNewTextInput((wlr_text_input_v3*)data);
}

void Events::listener_newSessionLock(wl_listener* listener, void* data) {
    Debug::log(LOG, "New session lock!");

    g_pSessionLockManager->onNewSessionLock((wlr_session_lock_v1*)data);
}

void Events::listener_setGamma(wl_listener* listener, void* data) {
    Debug::log(LOG, "New Gamma event at {:x}", (uintptr_t)data);

    const auto E = (wlr_gamma_control_manager_v1_set_gamma_event*)data;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(E->output);

    if (!PMONITOR) {
        Debug::log(ERR, "Gamma event object references non-existent output {:x} ?", (uintptr_t)E->output);
        return;
    }

    PMONITOR->gammaChanged = true;

    g_pCompositor->scheduleFrameForMonitor(PMONITOR);
}

void Events::listener_setCursorShape(wl_listener* listener, void* data) {
    const auto E = (wlr_cursor_shape_manager_v1_request_set_shape_event*)data;

    g_pInputManager->processMouseRequest(E);
}

void Events::listener_newTearingHint(wl_listener* listener, void* data) {
    Debug::log(LOG, "New tearing hint at {:x}", (uintptr_t)data);

    const auto NEWCTRL = g_pHyprRenderer->m_vTearingControllers.emplace_back(std::make_unique<STearingController>()).get();
    NEWCTRL->pWlrHint  = (wlr_tearing_control_v1*)data;

    NEWCTRL->hyprListener_destroy.initCallback(
        &NEWCTRL->pWlrHint->events.destroy,
        [&](void* owner, void* data) {
            Debug::log(LOG, "Destroyed {:x} tearing hint", (uintptr_t)((STearingController*)owner)->pWlrHint);

            std::erase_if(g_pHyprRenderer->m_vTearingControllers, [&](const auto& other) { return other.get() == owner; });
        },
        NEWCTRL, "TearingController");

    NEWCTRL->hyprListener_set.initCallback(
        &NEWCTRL->pWlrHint->events.set_hint,
        [&](void* owner, void* data) {
            const auto TEARINGHINT = (STearingController*)owner;

            const auto PWINDOW = g_pCompositor->getWindowFromSurface(TEARINGHINT->pWlrHint->surface);

            if (PWINDOW) {
                PWINDOW->m_bTearingHint = (bool)TEARINGHINT->pWlrHint->current;

                Debug::log(LOG, "Hint {:x} (window {}) set tearing hint to {}", (uintptr_t)TEARINGHINT->pWlrHint, PWINDOW, (uint32_t)TEARINGHINT->pWlrHint->current);
            }
        },
        NEWCTRL, "TearingController");
}
