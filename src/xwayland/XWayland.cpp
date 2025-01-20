#include "XWayland.hpp"
#include "../Compositor.hpp"
#include "../debug/Log.hpp"
#include "../helpers/fs/FsUtils.hpp"

CXWayland::CXWayland(const bool wantsEnabled) {
#ifndef NO_XWAYLAND
    // Disable Xwayland and clean up if the user disabled it.
    if (!wantsEnabled) {
        Debug::log(LOG, "XWayland has been disabled, cleaning up...");
        for (auto& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsX11)
                continue;
            g_pCompositor->closeWindow(w);
        }
        unsetenv("DISPLAY");
        m_enabled = false;
        return;
    }

    if (!NFsUtils::executableExistsInPath("Xwayland")) {
        // If Xwayland doesn't exist, don't try to start it.
        Debug::log(LOG, "Unable to find XWayland; not starting it.");
        return;
    }

    Debug::log(LOG, "Starting up the XWayland server");

    pServer = std::make_unique<CXWaylandServer>();

    if (!pServer->create()) {
        Debug::log(ERR, "XWayland failed to start: it will not work.");
        return;
    }

    m_enabled = true;
#else
    Debug::log(LOG, "Not starting XWayland: disabled at compile time");
#endif
}

void CXWayland::setCursor(unsigned char* pixData, uint32_t stride, const Vector2D& size, const Vector2D& hotspot) {
#ifndef NO_XWAYLAND
    if (!pWM) {
        Debug::log(ERR, "Couldn't set XCursor: no XWM yet");
        return;
    }

    pWM->setCursor(pixData, stride, size, hotspot);
#endif
}

bool CXWayland::enabled() {
    return m_enabled;
}
