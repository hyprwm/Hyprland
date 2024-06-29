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

void Events::listener_RendererDestroy(wl_listener* listener, void* data) {
    Debug::log(LOG, "!!Renderer destroyed!!");
}

void Events::listener_sessionActive(wl_listener* listener, void* data) {
    if (g_pCompositor->m_pAqBackend->session->active) {
        Debug::log(LOG, "Session got activated!");

        g_pCompositor->m_bSessionActive = true;

        for (auto& m : g_pCompositor->m_vMonitors) {
            g_pCompositor->scheduleFrameForMonitor(m.get());
            g_pHyprRenderer->applyMonitorRule(m.get(), &m->activeMonitorRule, true);
        }

        g_pConfigManager->m_bWantsMonitorReload = true;
    } else {
        Debug::log(LOG, "Session got inactivated!");

        g_pCompositor->m_bSessionActive = false;

        for (auto& m : g_pCompositor->m_vMonitors) {
            m->noFrameSchedule = true;
            m->framesToSkip    = 1;
        }
    }
}
