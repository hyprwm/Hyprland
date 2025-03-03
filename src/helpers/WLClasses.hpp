#pragma once

#include "../defines.hpp"
#include "../desktop/Subsurface.hpp"
#include "../desktop/Popup.hpp"
#include "../desktop/WLSurface.hpp"
#include "../macros.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "memory/Memory.hpp"
#include "signal/Signal.hpp"

class CMonitor;
class CPointer;
class CKeyboard;
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
