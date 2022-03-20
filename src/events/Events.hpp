#pragma once
#include "../defines.hpp"

namespace Events {
    // Monitor events
    LISTENER(change);
    LISTENER(newOutput);

    // Layer events
    LISTENER(newLayerSurface);
    LISTENER(destroyLayerSurface);
    LISTENER(mapLayerSurface);
    LISTENER(unmapLayerSurface);
    LISTENER(commitLayerSurface);

    // Popups
    LISTENER(newPopup);
    LISTENER(newPopupFromPopup);
    LISTENER(mapPopup);
    LISTENER(unmapPopup);
    LISTENER(destroyPopup);
    LISTENER(commitPopup);


    // Surface XDG (window)
    LISTENER(newXDGSurface);

    // Window events
    LISTENER(commitWindow);
    LISTENER(mapWindow);
    LISTENER(unmapWindow);
    LISTENER(destroyWindow);
    LISTENER(setTitleWindow);
    LISTENER(fullscreenWindow);
    LISTENER(activateX11);
    LISTENER(configureX11);

    // Input events
    LISTENER(mouseMove);
    LISTENER(mouseMoveAbsolute);
    LISTENER(mouseButton);
    LISTENER(mouseAxis);
    LISTENER(mouseFrame);
    
    LISTENER(newInput);

    LISTENER(keyboardKey);
    LISTENER(keyboardMod);
    LISTENER(keyboardDestroy);

    // Various
    LISTENER(requestMouse);
    LISTENER(requestSetSel);
    LISTENER(requestSetPrimarySel);
    LISTENER(activate);

    // outputMgr
    LISTENER(outputMgrApply);
    LISTENER(outputMgrTest);

    // Monitor part 2 the sequel
    LISTENER(monitorFrame);
    LISTENER(monitorDestroy);

    // XWayland
    LISTENER(readyXWayland);
    LISTENER(surfaceXWayland);
};