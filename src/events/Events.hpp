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

    // DRM events
    LISTENER(leaseRequest);

    // Layer events
    LISTENER(newLayerSurface);

    // Surface XDG (window)
    LISTENER(newXDGToplevel);

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
    DYNLISTENFUNC(setOverrideRedirect);
    DYNLISTENFUNC(associateX11);
    DYNLISTENFUNC(dissociateX11);
    DYNLISTENFUNC(ackConfigure);

    LISTENER(newInput);

    // Virt Ptr
    LISTENER(newVirtPtr);

    // Various
    LISTENER(requestMouse);
    LISTENER(requestSetSel);
    LISTENER(requestSetPrimarySel);

    // Monitor part 2 the sequel
    DYNLISTENFUNC(monitorFrame);
    DYNLISTENFUNC(monitorDestroy);
    DYNLISTENFUNC(monitorStateRequest);
    DYNLISTENFUNC(monitorDamage);
    DYNLISTENFUNC(monitorNeedsFrame);
    DYNLISTENFUNC(monitorCommit);
    DYNLISTENFUNC(monitorBind);

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

    // Renderer destroy
    LISTENER(RendererDestroy);

    // session
    LISTENER(sessionActive);

    // Session Lock
    LISTENER(newSessionLock);
};
