#include "XWayland.hpp"
#include "../debug/Log.hpp"

CXWayland::CXWayland(const bool enabled) {
#ifndef NO_XWAYLAND
    Debug::log(LOG, "Starting up the XWayland server");

    pServer = std::make_unique<CXWaylandServer>();

    if (!enabled) {
        unsetenv("DISPLAY");
        return;
    }

    if (!pServer->create()) {
        Debug::log(ERR, "XWayland failed to start: it will not work.");
        return;
    }
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
