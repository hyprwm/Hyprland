#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "../managers/CursorManager.hpp"

// ------------------------------ //
//   __  __ _____  _____  _____   //
//  |  \/  |_   _|/ ____|/ ____|  //
//  | \  / | | | | (___ | |       //
//  | |\/| | | |  \___ \| |       //
//  | |  | |_| |_ ____) | |____   //
//  |_|  |_|_____|_____/ \_____|  //
//                                //
// ------------------------------ //

void Events::listener_leaseRequest(wl_listener* listener, void* data) {
    const auto               REQUEST = (wlr_drm_lease_request_v1*)data;
    struct wlr_drm_lease_v1* lease   = wlr_drm_lease_request_v1_grant(REQUEST);
    if (!lease) {
        Debug::log(ERR, "Failed to grant lease request!");
        wlr_drm_lease_request_v1_reject(REQUEST);
    }
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

        free(reply);
    }

    //wlr_xwayland_set_seat(g_pXWaylandManager->m_sWLRXWayland, g_pCompositor->m_sSeat.seat);

    g_pCursorManager->setXWaylandCursor(g_pXWaylandManager->m_sWLRXWayland);

    const auto  ROOT   = xcb_setup_roots_iterator(xcb_get_setup(XCBCONNECTION)).data->root;
    auto        cookie = xcb_get_property(XCBCONNECTION, 0, ROOT, HYPRATOMS["_NET_SUPPORTING_WM_CHECK"], XCB_ATOM_ANY, 0, 2048);
    auto        reply  = xcb_get_property_reply(XCBCONNECTION, cookie, nullptr);

    const auto  XWMWINDOW = *(xcb_window_t*)xcb_get_property_value(reply);
    const char* name      = "Hyprland";

    xcb_change_property(wlr_xwayland_get_xwm_connection(g_pXWaylandManager->m_sWLRXWayland), XCB_PROP_MODE_REPLACE, XWMWINDOW, HYPRATOMS["_NET_WM_NAME"], HYPRATOMS["UTF8_STRING"],
                        8, // format
                        strlen(name), name);

    free(reply);

    xcb_disconnect(XCBCONNECTION);
#endif
}

void Events::listener_RendererDestroy(wl_listener* listener, void* data) {
    Debug::log(LOG, "!!Renderer destroyed!!");
}

void Events::listener_sessionActive(wl_listener* listener, void* data) {
    Debug::log(LOG, "Session got activated!");

    g_pCompositor->m_bSessionActive = true;

    for (auto& m : g_pCompositor->m_vMonitors) {
        g_pCompositor->scheduleFrameForMonitor(m.get());
        g_pHyprRenderer->applyMonitorRule(m.get(), &m->activeMonitorRule, true);
    }

    g_pConfigManager->m_bWantsMonitorReload = true;
}
