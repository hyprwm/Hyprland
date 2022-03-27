#pragma once
#include "../defines.hpp"

//
// LISTEN_NAME -> the wl_listener
//
// LISTENER_NAME -> the wl_listener.notify function
//

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

    // Subsurfaces
    LISTENER(newSubsurfaceNode);
    LISTENER(destroySubsurfaceNode);
    LISTENER(mapSubsurface);
    LISTENER(unmapSubsurface);
    LISTENER(destroySubsurface);
    LISTENER(commitSubsurface);

    // Popups
    LISTENER(newPopup); // LayerSurface

    LISTENER(newPopupXDG);
    LISTENER(mapPopupXDG);
    LISTENER(unmapPopupXDG);
    LISTENER(destroyPopupXDG);
    LISTENER(commitPopupXDG);
    LISTENER(newPopupFromPopupXDG);

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

    // Window subsurfaces
   // LISTENER(newSubsurfaceWindow);

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

    // Drag & Drop
    LISTENER(requestDrag);
    LISTENER(startDrag);

    // Inhibit
    LISTENER(InhibitActivate);
    LISTENER(InhibitDeactivate);
};