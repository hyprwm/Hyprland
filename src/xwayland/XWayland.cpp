#include "XWayland.hpp"
#include "../Compositor.hpp"
#include "../debug/log/Logger.hpp"
#include "../helpers/fs/FsUtils.hpp"

CXWayland::CXWayland(const bool wantsEnabled) {
#ifndef NO_XWAYLAND
    // Disable Xwayland and clean up if the user disabled it.
    if (!wantsEnabled) {
        LOG(Log::DEBUG, "XWayland has been disabled, cleaning up...");
        for (auto& w : Desktop::windowState()->windows()) {
            if (!w->backend().isX11())
                continue;
            w->sendClose();
        }
        unsetenv("DISPLAY");
        m_enabled = false;
        return;
    }

    if (!NFsUtils::executableExistsInPath("Xwayland")) {
        // If Xwayland doesn't exist, don't try to start it.
        LOG(Log::DEBUG, "Unable to find XWayland; not starting it.");
        return;
    }

    LOG(Log::DEBUG, "Starting up the XWayland server");

    m_server = makeUnique<CXWaylandServer>();

    if (!m_server->create()) {
        LOG(Log::ERR, "XWayland failed to start: it will not work.");
        return;
    }

    m_enabled = true;
#else
    LOG(Log::DEBUG, "Not starting XWayland: disabled at compile time");
#endif
}

void CXWayland::setCursor(unsigned char* pixData, uint32_t stride, const Vector2D& size, const Vector2D& hotspot) {
#ifndef NO_XWAYLAND
    if (!m_wm) {
        LOG(Log::ERR, "Couldn't set XCursor: no XWM yet");
        return;
    }

    m_wm->setCursor(pixData, stride, size, hotspot);
#endif
}

bool CXWayland::enabled() {
    return m_enabled;
}
