#include "TouchDevice.hpp"
#include "../defines.hpp"

SP<CTouchDevice> CTouchDevice::create(wlr_touch* touch) {
    SP<CTouchDevice> pTouch = SP<CTouchDevice>(new CTouchDevice(touch));

    pTouch->self = pTouch;

    return pTouch;
}

CTouchDevice::CTouchDevice(wlr_touch* touch_) : touch(touch_) {
    if (!touch)
        return;

    // clang-format off
    hyprListener_destroy.initCallback(&touch->base.events.destroy, [this] (void* owner, void* data) {
        events.destroy.emit();
        disconnectCallbacks();
        touch = nullptr;
    }, this, "CTouchDevice");

    hyprListener_down.initCallback(&touch->events.down, [this] (void* owner, void* data) {
        auto E = (wlr_touch_down_event*)data;

        touchEvents.down.emit(SDownEvent{
            .timeMs  = E->time_msec,
            .touchID = E->touch_id,
            .pos     = {E->x, E->y},
            .device  = self.lock(),
        });
    }, this, "CTouchDevice");

    hyprListener_up.initCallback(&touch->events.up, [this] (void* owner, void* data) {
        auto E = (wlr_touch_up_event*)data;

        touchEvents.up.emit(SUpEvent{
            .timeMs  = E->time_msec,
            .touchID = E->touch_id
        });
    }, this, "CTouchDevice");

    hyprListener_motion.initCallback(&touch->events.motion, [this] (void* owner, void* data) {
        auto E = (wlr_touch_motion_event*)data;

        touchEvents.motion.emit(SMotionEvent{
            .timeMs  = E->time_msec,
            .touchID = E->touch_id,
            .pos     = {E->x, E->y},
        });
    }, this, "CTouchDevice");

    hyprListener_cancel.initCallback(&touch->events.cancel, [this] (void* owner, void* data) {
        auto E = (wlr_touch_cancel_event*)data;

        touchEvents.cancel.emit(SCancelEvent{
            .timeMs  = E->time_msec,
            .touchID = E->touch_id
        });
    }, this, "CTouchDevice");

    hyprListener_frame.initCallback(&touch->events.frame, [this] (void* owner, void* data) {
        touchEvents.frame.emit();
    }, this, "CTouchDevice");

    // clang-format on

    deviceName = touch->base.name ? touch->base.name : "UNKNOWN";
}

bool CTouchDevice::isVirtual() {
    return false;
}

wlr_touch* CTouchDevice::wlr() {
    return touch;
}

void CTouchDevice::disconnectCallbacks() {
    hyprListener_destroy.removeCallback();
    hyprListener_down.removeCallback();
    hyprListener_up.removeCallback();
    hyprListener_motion.removeCallback();
    hyprListener_cancel.removeCallback();
    hyprListener_frame.removeCallback();
}
