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
    wlr_seat_set_primary_selection(g_pCompositor->m_sWLRSeat, EVENT->source, EVENT->serial);
}

void Events::listener_requestSetSel(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_request_set_selection_event*)data;
    wlr_seat_set_selection(g_pCompositor->m_sWLRSeat, EVENT->source, EVENT->serial);
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

    wlr_xwayland_set_seat(g_pXWaylandManager->m_sWLRXWayland, g_pCompositor->m_sWLRSeat);

    const auto XCURSOR = wlr_xcursor_manager_get_xcursor(g_pCompositor->m_sWLRXCursorMgr, "left_ptr", 1);
    if (XCURSOR) {
        wlr_xwayland_set_cursor(g_pXWaylandManager->m_sWLRXWayland, XCURSOR->images[0]->buffer, XCURSOR->images[0]->width * 4, XCURSOR->images[0]->width, XCURSOR->images[0]->height, XCURSOR->images[0]->hotspot_x, XCURSOR->images[0]->hotspot_y);
    }

    xcb_disconnect(XCBCONNECTION);
}