#pragma once

#include "IHID.hpp"
#include "../macros.hpp"
#include "../helpers/math/Math.hpp"

AQUAMARINE_FORWARD(ITouch);

class ITouch : public IHID {
  public:
    virtual uint32_t               getCapabilities();
    virtual eHIDType               getType();
    virtual bool                   isVirtual() = 0;
    virtual SP<Aquamarine::ITouch> aq()        = 0;

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
    } m_touchEvents;

    std::string m_boundOutput = "";

    WP<ITouch>  m_self;
};