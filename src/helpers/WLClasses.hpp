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
class IPointer;
class IKeyboard;
class CWLSurfaceResource;

AQUAMARINE_FORWARD(ISwitch);

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
