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
    DYNLISTENFUNC(destroyLayerSurface);
    DYNLISTENFUNC(mapLayerSurface);
    DYNLISTENFUNC(unmapLayerSurface);
    DYNLISTENFUNC(commitLayerSurface);

    // Subsurfaces
    DYNLISTENFUNC(newSubsurfaceNode);
    DYNLISTENFUNC(destroySubsurfaceNode);
    DYNLISTENFUNC(mapSubsurface);
    DYNLISTENFUNC(unmapSubsurface);
    DYNLISTENFUNC(destroySubsurface);
    DYNLISTENFUNC(commitSubsurface);

    // Popups
    DYNLISTENFUNC(newPopup);  // LayerSurface

    DYNLISTENFUNC(newPopupXDG);
    DYNLISTENFUNC(mapPopupXDG);
    DYNLISTENFUNC(unmapPopupXDG);
    DYNLISTENFUNC(destroyPopupXDG);
    DYNLISTENFUNC(commitPopupXDG);
    DYNLISTENFUNC(newPopupFromPopupXDG);

    // Surface XDG (window)
    LISTENER(newXDGSurface);

    // Window events
    DYNLISTENFUNC(commitWindow);
    DYNLISTENFUNC(mapWindow);
    DYNLISTENFUNC(unmapWindow);
    DYNLISTENFUNC(destroyWindow);
    DYNLISTENFUNC(setTitleWindow);
    DYNLISTENFUNC(fullscreenWindow);
    DYNLISTENFUNC(activateX11);
    DYNLISTENFUNC(configureX11);
    DYNLISTENFUNC(unmanagedSetGeometry);
    DYNLISTENFUNC(requestMove);
    DYNLISTENFUNC(requestResize);
    DYNLISTENFUNC(requestMinimize);
    DYNLISTENFUNC(requestMaximize);

    // Window subsurfaces
   // LISTENER(newSubsurfaceWindow);

    // Input events
    LISTENER(mouseMove);
    LISTENER(mouseMoveAbsolute);
    LISTENER(mouseButton);
    LISTENER(mouseAxis);
    LISTENER(mouseFrame);
    
    LISTENER(newInput);

    // Virt Ptr
    LISTENER(newVirtPtr);
    DYNLISTENFUNC(destroyMouse);

    DYNLISTENFUNC(keyboardKey);
    DYNLISTENFUNC(keyboardMod);
    DYNLISTENFUNC(keyboardDestroy);
    
    DYNLISTENFUNC(commitConstraint);
    LISTENER(newConstraint);
    DYNLISTENFUNC(setConstraintRegion);
    DYNLISTENFUNC(destroyConstraint);

    // Various
    LISTENER(requestMouse);
    LISTENER(requestSetSel);
    LISTENER(requestSetPrimarySel);
    DYNLISTENFUNC(activate);

    // outputMgr
    LISTENER(outputMgrApply);
    LISTENER(outputMgrTest);

    // Monitor part 2 the sequel
    DYNLISTENFUNC(monitorFrame);
    DYNLISTENFUNC(monitorDestroy);

    // XWayland
    LISTENER(readyXWayland);
    LISTENER(surfaceXWayland);

    // Drag & Drop
    LISTENER(requestDrag);
    LISTENER(startDrag);
    DYNLISTENFUNC(destroyDrag);

    DYNLISTENFUNC(mapDragIcon);
    DYNLISTENFUNC(unmapDragIcon);
    DYNLISTENFUNC(destroyDragIcon);
    DYNLISTENFUNC(commitDragIcon);

    // Inhibit
    LISTENER(InhibitActivate);
    LISTENER(InhibitDeactivate);

    // Deco XDG
    LISTENER(NewXDGDeco);

    // Renderer destroy
    LISTENER(RendererDestroy);

    LISTENER(newIdleInhibitor);

    // session
    LISTENER(sessionActive);

    // Touchpad shit
    LISTENER(swipeBegin);
    LISTENER(swipeEnd);
    LISTENER(swipeUpdate);
    LISTENER(pinchBegin);
    LISTENER(pinchUpdate);
    LISTENER(pinchEnd);
};