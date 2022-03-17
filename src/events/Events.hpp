#pragma once
#include "../defines.hpp"

namespace Events {
    LISTENER(activate);
    LISTENER(change);
    LISTENER(newOutput);

    LISTENER(newLayerSurface);
    LISTENER(destroyLayerSurface);
    LISTENER(mapLayerSurface);
    LISTENER(unmapLayerSurface);
    LISTENER(commitLayerSurface);

    LISTENER(newXDGSurface);

    LISTENER(commitWindow);
    LISTENER(mapWindow);
    LISTENER(unmapWindow);
    LISTENER(destroyWindow);
    LISTENER(setTitleWindow);
    LISTENER(fullscreenWindow);

    LISTENER(mouseMove);
    LISTENER(mouseMoveAbsolute);
    LISTENER(mouseButton);
    LISTENER(mouseAxis);
    LISTENER(mouseFrame);
    
    LISTENER(newInput);

    LISTENER(keyboardKey);
    LISTENER(keyboardMod);
    LISTENER(keyboardDestroy);

    LISTENER(requestMouse);
    LISTENER(requestSetSel);
    LISTENER(requestSetPrimarySel);

    LISTENER(outputMgrApply);
    LISTENER(outputMgrTest);

    LISTENER(monitorFrame);
    LISTENER(monitorDestroy);
};