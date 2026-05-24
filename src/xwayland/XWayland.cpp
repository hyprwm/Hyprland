#include "XWayland.hpp"
#include "../Compositor.hpp"
#include "../debug/log/Logger.hpp"
#include "../helpers/fs/FsUtils.hpp"

CXWayland::CXWayland(const bool wantsEnabled) {
#ifndef NO_XWAYLAND

#ifdef USE_XWAYLAND_SATELLITE
    // xwayland-satellite mode
    if (!wantsEnabled) {
        Log::logger->log(Log::DEBUG, "XWayland has been disabled, cleaning up...");
        unsetenv("DISPLAY");
        m_enabled = false;
        return;
    }

    Log::logger->log(Log::DEBUG, "Starting XWayland via xwayland-satellite");

    m_satellite = makeUnique<CXWaylandSatellite>();

    if (!m_satellite->setup(g_pCompositor->m_wlEventLoop)) {
        Log::logger->log(Log::WARN, "XWayland satellite failed to set up: X11 apps will not work.");
        m_satellite.reset();
        return;
    }

    m_enabled = true;

#else
    // Built-in XWayland mode (legacy)
    // Disable Xwayland and clean up if the user disabled it.
    if (!wantsEnabled) {
        Log::logger->log(Log::DEBUG, "XWayland has been disabled, cleaning up...");
        for (auto& w : g_pCompositor->m_windows) {
            if (!w->m_isX11)
                continue;
            w->sendClose();
        }
        unsetenv("DISPLAY");
        m_enabled = false;
        return;
    }

    if (!NFsUtils::executableExistsInPath("Xwayland")) {
        // If Xwayland doesn't exist, don't try to start it.
        Log::logger->log(Log::DEBUG, "Unable to find XWayland; not starting it.");
        return;
    }

    Log::logger->log(Log::DEBUG, "Starting up the XWayland server");

    m_server = makeUnique<CXWaylandServer>();

    if (!m_server->create()) {
        Log::logger->log(Log::ERR, "XWayland failed to start: it will not work.");
        return;
    }

    m_enabled = true;
#endif

#else
    Log::logger->log(Log::DEBUG, "Not starting XWayland: disabled at compile time");
#endif
}

#ifndef USE_XWAYLAND_SATELLITE
void CXWayland::setCursor(unsigned char* pixData, uint32_t stride, const Vector2D& size, const Vector2D& hotspot) {
#ifndef NO_XWAYLAND
    if (!m_wm) {
        Log::logger->log(Log::ERR, "Couldn't set XCursor: no XWM yet");
        return;
    }

    m_wm->setCursor(pixData, stride, size, hotspot);
#endif
}
#endif

bool CXWayland::enabled() {
    return m_enabled;
}
