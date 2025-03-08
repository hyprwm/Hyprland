#pragma once

#include "IHID.hpp"
#include "../macros.hpp"
#include "../helpers/math/Math.hpp"

AQUAMARINE_FORWARD(IPointer);

/*
    Base class for a pointer.
*/
class IPointer : public IHID {
  public:
    virtual uint32_t                 getCapabilities();
    virtual eHIDType                 getType();
    virtual bool                     isVirtual() = 0;
    virtual SP<Aquamarine::IPointer> aq()        = 0;

    struct SMotionEvent {
        uint32_t     timeMs = 0;
        Vector2D     delta, unaccel;
        bool         mouse = false;
        SP<IPointer> device;
    };

    struct SMotionAbsoluteEvent {
        uint32_t timeMs = 0;
        Vector2D absolute; // 0.0 - 1.0
        SP<IHID> device;
    };

    struct SButtonEvent {
        uint32_t                timeMs = 0;
        uint32_t                button = 0;
        wl_pointer_button_state state  = WL_POINTER_BUTTON_STATE_PRESSED;
        bool                    mouse  = false;
    };

    struct SAxisEvent {
        uint32_t                           timeMs            = 0;
        wl_pointer_axis_source             source            = WL_POINTER_AXIS_SOURCE_WHEEL;
        wl_pointer_axis                    axis              = WL_POINTER_AXIS_VERTICAL_SCROLL;
        wl_pointer_axis_relative_direction relativeDirection = WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL;
        double                             delta             = 0.0;
        int32_t                            deltaDiscrete     = 0;
        bool                               mouse             = false;
    };

    struct SSwipeBeginEvent {
        uint32_t timeMs  = 0;
        uint32_t fingers = 0;
    };

    struct SSwipeUpdateEvent {
        uint32_t timeMs  = 0;
        uint32_t fingers = 0;
        Vector2D delta;
    };

    struct SSwipeEndEvent {
        uint32_t timeMs    = 0;
        bool     cancelled = false;
    };

    struct SPinchBeginEvent {
        uint32_t timeMs  = 0;
        uint32_t fingers = 0;
    };

    struct SPinchUpdateEvent {
        uint32_t timeMs  = 0;
        uint32_t fingers = 0;
        Vector2D delta;
        double   scale = 1.0, rotation = 0.0;
    };

    struct SPinchEndEvent {
        uint32_t timeMs    = 0;
        bool     cancelled = false;
    };

    struct SHoldBeginEvent {
        uint32_t timeMs  = 0;
        uint32_t fingers = 0;
    };

    struct SHoldEndEvent {
        uint32_t timeMs    = 0;
        bool     cancelled = false;
    };

    struct {
        CSignal motion;
        CSignal motionAbsolute;
        CSignal button;
        CSignal axis;
        CSignal frame;

        CSignal swipeBegin;
        CSignal swipeEnd;
        CSignal swipeUpdate;

        CSignal pinchBegin;
        CSignal pinchEnd;
        CSignal pinchUpdate;

        CSignal holdBegin;
        CSignal holdEnd;
    } pointerEvents;

    bool         connected   = false; // means connected to the cursor
    std::string  boundOutput = "";
    bool         flipX       = false; // decide to invert horizontal movement
    bool         flipY       = false; // decide to invert vertical movement
    bool         isTouchpad  = false;
    bool         invert_axis = false;

    WP<IPointer> self;
};
