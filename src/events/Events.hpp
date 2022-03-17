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

    LISTENER(mouseMove);
    LISTENER(mouseMoveAbsolute);
    LISTENER(mouseButton);
    LISTENER(mouseAxis);
    LISTENER(mouseFrame);
    LISTENER(newInput);
    LISTENER(newKeyboard);

    LISTENER(requestMouse);
    LISTENER(requestSetSel);
    LISTENER(requestSetPrimarySel);

    LISTENER(outputMgrApply);
    LISTENER(outputMgrTest);

    LISTENER(monitorFrame);
    LISTENER(monitorDestroy);
};