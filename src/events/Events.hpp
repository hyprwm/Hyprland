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
    DYNLISTENFUNC(ackConfigure);

    LISTENER(newInput);

    // Virt Ptr
    LISTENER(newVirtPtr);

    // Various
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
    LISTENER(surfaceXWayland);

    // Renderer destroy
    LISTENER(RendererDestroy);

    // session
    LISTENER(sessionActive);

    // Session Lock
    LISTENER(newSessionLock);
};
