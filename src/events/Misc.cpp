#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/InputManager.hpp"
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

void Events::listener_requestSetPrimarySel(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_request_set_primary_selection_event*)data;
    wlr_seat_set_primary_selection(g_pCompositor->m_sSeat.seat, EVENT->source, EVENT->serial);
}

void Events::listener_requestSetSel(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_request_set_selection_event*)data;
    wlr_seat_set_selection(g_pCompositor->m_sSeat.seat, EVENT->source, EVENT->serial);
}

void Events::listener_readyXWayland(wl_listener* listener, void* data) {
    const auto XCBCONNECTION = xcb_connect(g_pXWaylandManager->m_sWLRXWayland->display_name, NULL);
    const auto ERR = xcb_connection_has_error(XCBCONNECTION);
    if (ERR) {
        Debug::log(LogLevel::ERR, "XWayland -> xcb_connection_has_error failed with %i", ERR);
        return;
    }

    for (auto& ATOM : HYPRATOMS) {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(XCBCONNECTION, 0, ATOM.first.length(), ATOM.first.c_str());
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(XCBCONNECTION, cookie, NULL);

        if (!reply) {
            Debug::log(LogLevel::ERR, "XWayland -> Atom failed: %s", ATOM.first.c_str());
            continue;
        }

        ATOM.second = reply->atom;
    }

    wlr_xwayland_set_seat(g_pXWaylandManager->m_sWLRXWayland, g_pCompositor->m_sSeat.seat);

    const auto XCURSOR = wlr_xcursor_manager_get_xcursor(g_pCompositor->m_sWLRXCursorMgr, "left_ptr", 1);
    if (XCURSOR) {
        wlr_xwayland_set_cursor(g_pXWaylandManager->m_sWLRXWayland, XCURSOR->images[0]->buffer, XCURSOR->images[0]->width * 4, XCURSOR->images[0]->width, XCURSOR->images[0]->height, XCURSOR->images[0]->hotspot_x, XCURSOR->images[0]->hotspot_y);
    }

    xcb_disconnect(XCBCONNECTION);
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
    // TODO: draw the drag icon
}

void Events::listener_InhibitActivate(wl_listener* listener, void* data) {
    g_pCompositor->m_sSeat.exclusiveClient = g_pCompositor->m_sWLRInhibitMgr->active_client;

    Debug::log(LOG, "Activated exclusive for %x.", g_pCompositor->m_sSeat.exclusiveClient);
}

void Events::listener_InhibitDeactivate(wl_listener* listener, void* data) {
    g_pCompositor->m_sSeat.exclusiveClient = nullptr;
    g_pInputManager->refocus();

    Debug::log(LOG, "Deactivated exclusive.");
}