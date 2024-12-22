#pragma once

#include "../events/Events.hpp"
#include "../defines.hpp"
#include "../desktop/Window.hpp"
#include "../desktop/Subsurface.hpp"
#include "../desktop/Popup.hpp"
#include "AnimatedVariable.hpp"
#include "../desktop/WLSurface.hpp"
#include "signal/Signal.hpp"
#include "math/Math.hpp"

class CMonitor;
class IPointer;
class IKeyboard;
class CWLSurfaceResource;

AQUAMARINE_FORWARD(ISwitch);

struct SSwipeGesture {
    PHLWORKSPACE  pWorkspaceBegin = nullptr;

    double        delta = 0;

    int           initialDirection = 0;
    float         avgSpeed         = 0;
    int           speedPoints      = 0;
    int           touch_id         = 0;

    PHLMONITORREF pMonitor;
};

struct SSwitchDevice {
    WP<Aquamarine::ISwitch> pDevice;

    struct {
        CHyprSignalListener destroy;
        CHyprSignalListener fire;
    } listeners;

    bool operator==(const SSwitchDevice& other) const {
        return pDevice == other.pDevice;
    }
};
