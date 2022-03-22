#pragma once

#include "../events/Events.hpp"
#include "../defines.hpp"
#include "../../wlr-layer-shell-unstable-v1-protocol.h"
#include "../Window.hpp"

struct SLayerSurface {
    wlr_layer_surface_v1*   layerSurface;
    wl_list                 link;

    DYNLISTENER(destroyLayerSurface);
    DYNLISTENER(mapLayerSurface);
    DYNLISTENER(unmapLayerSurface);
    DYNLISTENER(commitLayerSurface);
    DYNLISTENER(newPopup);
    DYNLISTENER(newSubsurface);

    wlr_box                 geometry;
    zwlr_layer_shell_v1_layer layer;

    int                     monitorID = -1;


    // For the list lookup
    bool operator==(const SLayerSurface& rhs) {
        return layerSurface == rhs.layerSurface && monitorID == rhs.monitorID;
    }
};

struct SSubsurface {
    wlr_subsurface*     subsurface = nullptr;
    SLayerSurface*      pParentSurface = nullptr;

    DYNLISTENER(mapSubsurface);
    DYNLISTENER(unmapSubsurface);
    DYNLISTENER(destroySubsurface);
    DYNLISTENER(commitSubsurface);
    DYNLISTENER(newSubsurface);

    // For the list lookup
    bool operator==(const SSubsurface& rhs) {
        return subsurface == rhs.subsurface && pParentSurface == rhs.pParentSurface;
    }
};

struct SRenderData {
    wlr_output* output;
    timespec* when;
    int x, y;

    // for iters
    void* data = nullptr;
    wlr_surface* surface = nullptr;
    int w, h;
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

struct SLayerPopup {
    wlr_xdg_popup*  popup = nullptr;
    SLayerSurface*  parentSurface = nullptr;
    wlr_xdg_popup*  parentPopup = nullptr;

    DYNLISTENER(mapPopup);
    DYNLISTENER(destroyPopup);
    DYNLISTENER(unmapPopup);
    DYNLISTENER(commitPopup);
    DYNLISTENER(newPopupFromPopup);

    // For the list lookup
    bool operator==(const SLayerPopup& rhs) {
        return popup == rhs.popup;
    }
};

struct SXDGPopup {
    CWindow*        parentWindow = nullptr;
    wlr_xdg_popup*  parentPopup = nullptr;
    wlr_xdg_popup*  popup = nullptr;

    DYNLISTENER(newPopupFromPopupXDG);
    DYNLISTENER(destroyPopupXDG);
    DYNLISTENER(mapPopupXDG);
    DYNLISTENER(unmapPopupXDG);

    // For the list lookup
    bool operator==(const SXDGPopup& rhs) {
        return popup == rhs.popup;
    }
};

struct SSeat {
    wlr_seat*       seat = nullptr;
    wl_client*      exclusiveClient = nullptr;
};