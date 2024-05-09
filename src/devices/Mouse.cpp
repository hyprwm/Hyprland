#include "Mouse.hpp"
#include "../defines.hpp"

SP<CMouse> CMouse::create(wlr_pointer* mouse) {
    SP<CMouse> pMouse = SP<CMouse>(new CMouse(mouse));

    pMouse->self = pMouse;

    return pMouse;
}

CMouse::CMouse(wlr_pointer* mouse_) : mouse(mouse_) {
    if (!mouse)
        return;

    // clang-format off
    hyprListener_destroy.initCallback(&mouse->base.events.destroy, [this] (void* owner, void* data) {
        disconnectCallbacks();
        mouse = nullptr;
        events.destroy.emit();
    }, this, "CMouse");

    hyprListener_motion.initCallback(&mouse->events.motion, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_motion_event*)data;

        pointerEvents.motion.emit(SMotionEvent{
            .timeMs  = E->time_msec,
            .delta   = {E->delta_x, E->delta_y},
            .unaccel = {E->unaccel_dx, E->unaccel_dy},
        });
    }, this, "CMouse");

    hyprListener_motionAbsolute.initCallback(&mouse->events.motion_absolute, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_motion_absolute_event*)data;

        pointerEvents.motionAbsolute.emit(SMotionAbsoluteEvent{
            .timeMs   = E->time_msec,
            .absolute = {E->x, E->y},
            .device   = self.lock(),
        });
    }, this, "CMouse");

    hyprListener_button.initCallback(&mouse->events.button, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_button_event*)data;

        pointerEvents.button.emit(SButtonEvent{
            .timeMs = E->time_msec,
            .button = E->button,
            .state  = (wl_pointer_button_state)E->state,
        });
    }, this, "CMouse");

    hyprListener_axis.initCallback(&mouse->events.axis, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_axis_event*)data;

        pointerEvents.axis.emit(SAxisEvent{
            .timeMs            = E->time_msec,
            .source            = E->source,
            .axis              = E->orientation,
            .relativeDirection = E->relative_direction,
            .delta             = E->delta,
            .deltaDiscrete     = E->delta_discrete,
        });
    }, this, "CMouse");

    hyprListener_frame.initCallback(&mouse->events.frame, [this] (void* owner, void* data) {
        pointerEvents.frame.emit();
    }, this, "CMouse");

    hyprListener_swipeBegin.initCallback(&mouse->events.swipe_begin, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_swipe_begin_event*)data;

        pointerEvents.swipeBegin.emit(SSwipeBeginEvent{
            .timeMs  = E->time_msec,
            .fingers = E->fingers,
        });
    }, this, "CMouse");

    hyprListener_swipeEnd.initCallback(&mouse->events.swipe_end, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_swipe_end_event*)data;

        pointerEvents.swipeEnd.emit(SSwipeEndEvent{
            .timeMs    = E->time_msec,
            .cancelled = E->cancelled,
        });
    }, this, "CMouse");

    hyprListener_swipeUpdate.initCallback(&mouse->events.swipe_update, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_swipe_update_event*)data;

        pointerEvents.swipeUpdate.emit(SSwipeUpdateEvent{
            .timeMs  = E->time_msec,
            .fingers = E->fingers,
            .delta   = {E->dx, E->dy},
        });
    }, this, "CMouse");

    hyprListener_pinchBegin.initCallback(&mouse->events.pinch_begin, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_pinch_begin_event*)data;

        pointerEvents.pinchBegin.emit(SPinchBeginEvent{
            .timeMs    = E->time_msec,
            .fingers   = E->fingers,
        });
    }, this, "CMouse");

    hyprListener_pinchEnd.initCallback(&mouse->events.pinch_end, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_pinch_end_event*)data;

        pointerEvents.pinchEnd.emit(SPinchEndEvent{
            .timeMs    = E->time_msec,
            .cancelled = E->cancelled,
        });
    }, this, "CMouse");

    hyprListener_pinchUpdate.initCallback(&mouse->events.pinch_update, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_pinch_update_event*)data;

        pointerEvents.pinchUpdate.emit(SPinchUpdateEvent{
            .timeMs   = E->time_msec,
            .fingers  = E->fingers,
            .delta    = {E->dx, E->dy},
            .scale    = E->scale,
            .rotation = E->rotation,
        });
    }, this, "CMouse");

    hyprListener_holdBegin.initCallback(&mouse->events.hold_begin, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_hold_begin_event*)data;

        pointerEvents.holdBegin.emit(SHoldBeginEvent{
            .timeMs  = E->time_msec,
            .fingers = E->fingers,
        });
    }, this, "CMouse");

    hyprListener_holdEnd.initCallback(&mouse->events.hold_end, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_hold_end_event*)data;

        pointerEvents.holdEnd.emit(SHoldEndEvent{
            .timeMs    = E->time_msec,
            .cancelled = E->cancelled,
        });
    }, this, "CMouse");

    // clang-format on

    deviceName = mouse->base.name ? mouse->base.name : "UNKNOWN";
}

void CMouse::disconnectCallbacks() {
    hyprListener_destroy.removeCallback();
    hyprListener_motion.removeCallback();
    hyprListener_motionAbsolute.removeCallback();
    hyprListener_button.removeCallback();
    hyprListener_axis.removeCallback();
    hyprListener_frame.removeCallback();
    hyprListener_swipeBegin.removeCallback();
    hyprListener_swipeEnd.removeCallback();
    hyprListener_swipeUpdate.removeCallback();
    hyprListener_pinchBegin.removeCallback();
    hyprListener_pinchEnd.removeCallback();
    hyprListener_pinchUpdate.removeCallback();
    hyprListener_holdBegin.removeCallback();
    hyprListener_holdEnd.removeCallback();
}

bool CMouse::isVirtual() {
    return false;
}

wlr_pointer* CMouse::wlr() {
    return mouse;
}
