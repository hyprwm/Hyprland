#pragma once
#include "../defines.hpp"

//
// LISTEN_NAME -> the wl_listener
//
// LISTENER_NAME -> the wl_listener.notify function
//

namespace Events {
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

    // Monitor part 2 the sequel
    DYNLISTENFUNC(monitorFrame);
    DYNLISTENFUNC(monitorStateRequest);
    DYNLISTENFUNC(monitorDamage);
    DYNLISTENFUNC(monitorNeedsFrame);
    DYNLISTENFUNC(monitorCommit);
    DYNLISTENFUNC(monitorBind);
};
