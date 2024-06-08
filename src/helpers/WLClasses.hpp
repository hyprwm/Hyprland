#pragma once

#include "../events/Events.hpp"
#include "../defines.hpp"
#include "../desktop/Window.hpp"
#include "../desktop/Subsurface.hpp"
#include "../desktop/Popup.hpp"
#include "AnimatedVariable.hpp"
#include "../desktop/WLSurface.hpp"
#include "signal/Listener.hpp"
#include "Region.hpp"

class CMonitor;
class IPointer;
class IKeyboard;
class CWLSurfaceResource;

struct SRenderData {
    CMonitor* pMonitor;
    timespec* when;
    double    x, y;

    // for iters
    void*                  data    = nullptr;
    SP<CWLSurfaceResource> surface = nullptr;
    double                 w, h;

    // for rounding
    bool dontRound = true;

    // for fade
    float fadeAlpha = 1.f;

    // for alpha settings
    float alpha = 1.f;

    // for decorations (border)
    bool decorate = false;

    // for custom round values
    int rounding = -1; // -1 means not set

    // for blurring
    bool blur                  = false;
    bool blockBlurOptimization = false;

    // only for windows, not popups
    bool squishOversized = true;

    // for calculating UV
    PHLWINDOW pWindow;

    bool      popup = false;
};

struct SSwipeGesture {
    PHLWORKSPACE pWorkspaceBegin = nullptr;

    double       delta = 0;

    int          initialDirection = 0;
    float        avgSpeed         = 0;
    int          speedPoints      = 0;
    int          touch_id         = 0;

    CMonitor*    pMonitor = nullptr;
};

struct SSwitchDevice {
    wlr_input_device* pWlrDevice = nullptr;

    int               status = -1; // uninitialized

    DYNLISTENER(destroy);
    DYNLISTENER(toggle);

    bool operator==(const SSwitchDevice& other) const {
        return pWlrDevice == other.pWlrDevice;
    }
};
