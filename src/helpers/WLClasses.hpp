#pragma once

#include "../events/Events.hpp"
#include "../defines.hpp"
#include "../../wlr-layer-shell-unstable-v1-protocol.h"

struct SLayerSurface {
    wlr_layer_surface_v1*   layerSurface;
    wl_list                 link;

    DYNLISTENER(destroyLayerSurface);
    DYNLISTENER(mapLayerSurface);
    DYNLISTENER(unmapLayerSurface);
    DYNLISTENER(commitLayerSurface);

    wlr_box                 geometry;
    zwlr_layer_shell_v1_layer layer;

    int                     monitorID = -1;


    // For the list lookup
    bool operator==(const SLayerSurface& rhs) {
        return layerSurface == rhs.layerSurface && monitorID == rhs.monitorID;
    }
};

struct SRenderData {
    wlr_output* output;
    timespec* when;
    int x;
    int y;
};

struct SKeyboard {
    wlr_input_device* keyboard;

    DYNLISTENER(keyboardMod);
    DYNLISTENER(keyboardKey);
    DYNLISTENER(keyboardDestroy);

    // For the list lookup
    bool operator==(const SKeyboard& rhs) {
        return keyboard == rhs.keyboard;
    }
};