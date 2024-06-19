#pragma once

#include "IHID.hpp"
#include "../helpers/WLListener.hpp"
#include "../macros.hpp"
#include "../helpers/math/Math.hpp"

struct wlr_touch;

class ITouch : public IHID {
  public:
    virtual uint32_t   getCapabilities();
    virtual eHIDType   getType();
    virtual bool       isVirtual() = 0;
    virtual wlr_touch* wlr()       = 0;

    struct SDownEvent {
        uint32_t   timeMs  = 0;
        int32_t    touchID = 0;
        Vector2D   pos;
        SP<ITouch> device;
    };

    struct SUpEvent {
        uint32_t timeMs  = 0;
        int32_t  touchID = 0;
    };

    struct SMotionEvent {
        uint32_t timeMs  = 0;
        int32_t  touchID = 0;
        Vector2D pos;
    };

    struct SCancelEvent {
        uint32_t timeMs  = 0;
        int32_t  touchID = 0;
    };

    struct {
        CSignal down;
        CSignal up;
        CSignal motion;
        CSignal cancel;
        CSignal frame;
    } touchEvents;

    std::string hlName      = "";
    std::string boundOutput = "";

    WP<ITouch>  self;
};