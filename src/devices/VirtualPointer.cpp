#include "VirtualPointer.hpp"
#include "../protocols/VirtualPointer.hpp"

SP<CVirtualPointer> CVirtualPointer::create(SP<CVirtualPointerV1Resource> resource) {
    SP<CVirtualPointer> pPointer = SP<CVirtualPointer>(new CVirtualPointer(resource));

    pPointer->self = pPointer;

    return pPointer;
}

CVirtualPointer::CVirtualPointer(SP<CVirtualPointerV1Resource> resource) : pointer(resource) {
    if (!resource->good())
        return;

    auto mouse = resource->wlr();

    // clang-format off
    hyprListener_destroy.initCallback(&mouse->base.events.destroy, [this] (void* owner, void* data) {
        disconnectCallbacks();
        events.destroy.emit();
    }, this, "CVirtualPointer");

    hyprListener_motion.initCallback(&mouse->events.motion, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_motion_event*)data;

        pointerEvents.motion.emit(SMotionEvent{
            .timeMs  = E->time_msec,
            .delta   = {E->delta_x, E->delta_y},
            .unaccel = {E->unaccel_dx, E->unaccel_dy},
        });
    }, this, "CVirtualPointer");

    hyprListener_motionAbsolute.initCallback(&mouse->events.motion_absolute, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_motion_absolute_event*)data;

        pointerEvents.motionAbsolute.emit(SMotionAbsoluteEvent{
            .timeMs   = E->time_msec,
            .absolute = {E->x, E->y},
            .device   = self.lock(),
        });
    }, this, "CVirtualPointer");

    hyprListener_button.initCallback(&mouse->events.button, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_button_event*)data;

        pointerEvents.button.emit(SButtonEvent{
            .timeMs = E->time_msec,
            .button = E->button,
            .state  = (wl_pointer_button_state)E->state,
        });
    }, this, "CVirtualPointer");

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
    }, this, "CVirtualPointer");

    hyprListener_frame.initCallback(&mouse->events.frame, [this] (void* owner, void* data) {
        pointerEvents.frame.emit();
    }, this, "CVirtualPointer");

    hyprListener_swipeBegin.initCallback(&mouse->events.swipe_begin, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_swipe_begin_event*)data;

        pointerEvents.swipeBegin.emit(SSwipeBeginEvent{
            .timeMs  = E->time_msec,
            .fingers = E->fingers,
        });
    }, this, "CVirtualPointer");

    hyprListener_swipeEnd.initCallback(&mouse->events.swipe_end, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_swipe_end_event*)data;

        pointerEvents.swipeEnd.emit(SSwipeEndEvent{
            .timeMs    = E->time_msec,
            .cancelled = E->cancelled,
        });
    }, this, "CVirtualPointer");

    hyprListener_swipeUpdate.initCallback(&mouse->events.swipe_update, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_swipe_update_event*)data;

        pointerEvents.swipeUpdate.emit(SSwipeUpdateEvent{
            .timeMs  = E->time_msec,
            .fingers = E->fingers,
            .delta   = {E->dx, E->dy},
        });
    }, this, "CVirtualPointer");

    hyprListener_pinchBegin.initCallback(&mouse->events.pinch_begin, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_pinch_begin_event*)data;

        pointerEvents.pinchBegin.emit(SPinchBeginEvent{
            .timeMs    = E->time_msec,
            .fingers   = E->fingers,
        });
    }, this, "CVirtualPointer");

    hyprListener_pinchEnd.initCallback(&mouse->events.pinch_end, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_pinch_end_event*)data;

        pointerEvents.pinchEnd.emit(SPinchEndEvent{
            .timeMs    = E->time_msec,
            .cancelled = E->cancelled,
        });
    }, this, "CVirtualPointer");

    hyprListener_pinchUpdate.initCallback(&mouse->events.pinch_update, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_pinch_update_event*)data;

        pointerEvents.pinchUpdate.emit(SPinchUpdateEvent{
            .timeMs   = E->time_msec,
            .fingers  = E->fingers,
            .delta    = {E->dx, E->dy},
            .scale    = E->scale,
            .rotation = E->rotation,
        });
    }, this, "CVirtualPointer");

    hyprListener_holdBegin.initCallback(&mouse->events.hold_begin, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_hold_begin_event*)data;

        pointerEvents.holdBegin.emit(SHoldBeginEvent{
            .timeMs  = E->time_msec,
            .fingers = E->fingers,
        });
    }, this, "CVirtualPointer");

    hyprListener_holdEnd.initCallback(&mouse->events.hold_end, [this] (void* owner, void* data) {
        auto E = (wlr_pointer_hold_end_event*)data;

        pointerEvents.holdEnd.emit(SHoldEndEvent{
            .timeMs    = E->time_msec,
            .cancelled = E->cancelled,
        });
    }, this, "CVirtualPointer");

    // clang-format on

    deviceName = mouse->base.name ? mouse->base.name : "UNKNOWN";
}

bool CVirtualPointer::isVirtual() {
    return true;
}

void CVirtualPointer::disconnectCallbacks() {
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

wlr_pointer* CVirtualPointer::wlr() {
    if (pointer.expired())
        return nullptr;
    return pointer->wlr();
}
