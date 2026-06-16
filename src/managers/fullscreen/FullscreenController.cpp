
#include "FullscreenController.hpp"

using namespace Fullscreen;

// ERSTARR TODO - Check modes. if synced, good.
// if not, set the one that's not synced.
// internal call will dispatch to the full FS pipeline

// This is done in the controller, not the handler. hanlers do as they are told (set internal/client)

if (WINDOW->m_ruleApplicator->syncFullscreen().valueOrDefault()) {
    setWindowFullscreenModeInternal(WINDOW, request.mode);
    setWindowFullscreenModeClient(WINDOW, request.mode);
}
