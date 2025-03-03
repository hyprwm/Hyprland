#include "Mouse.hpp"
#include "../defines.hpp"
#include <aquamarine/input/Input.hpp>

SP<CMouse> CMouse::create(SP<Aquamarine::IPointer> mouse) {
    SP<CMouse> pMouse = SP<CMouse>(new CMouse(mouse));

    pMouse->self = pMouse;

    return pMouse;
}

CMouse::CMouse(SP<Aquamarine::IPointer> mouse_) : mouse(mouse_) {
    if (!mouse)
        return;

    listeners.destroy = mouse->events.destroy.registerListener([this](std::any d) {
        mouse.reset();
        events.destroy.emit();
    });

    listeners.motion = mouse->events.move.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SMoveEvent>(d);

        pointerEvents.motion.emit(SMotionEvent{
            .timeMs  = E.timeMs,
            .delta   = E.delta,
            .unaccel = E.unaccel,
            .mouse   = true,
            .device  = self.lock(),
        });
    });

    listeners.motionAbsolute = mouse->events.warp.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SWarpEvent>(d);

        pointerEvents.motionAbsolute.emit(SMotionAbsoluteEvent{
            .timeMs   = E.timeMs,
            .absolute = E.absolute,
            .device   = self.lock(),
        });
    });

    listeners.button = mouse->events.button.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SButtonEvent>(d);

        pointerEvents.button.emit(SButtonEvent{
            .timeMs = E.timeMs,
            .button = E.button,
            .state  = E.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED,
            .mouse  = true,
        });
    });

    listeners.axis = mouse->events.axis.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SAxisEvent>(d);

        pointerEvents.axis.emit(SAxisEvent{
            .timeMs            = E.timeMs,
            .source            = (wl_pointer_axis_source)E.source,
            .axis              = (wl_pointer_axis)E.axis,
            .relativeDirection = (wl_pointer_axis_relative_direction)E.direction,
            .delta             = E.delta,
            .deltaDiscrete     = E.discrete,
            .mouse             = true,
        });
    });

    listeners.frame = mouse->events.frame.registerListener([this](std::any d) { pointerEvents.frame.emit(); });

    listeners.swipeBegin = mouse->events.swipeBegin.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SSwipeBeginEvent>(d);

        pointerEvents.swipeBegin.emit(SSwipeBeginEvent{
            .timeMs  = E.timeMs,
            .fingers = E.fingers,
        });
    });

    listeners.swipeEnd = mouse->events.swipeEnd.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SSwipeEndEvent>(d);

        pointerEvents.swipeEnd.emit(SSwipeEndEvent{
            .timeMs    = E.timeMs,
            .cancelled = E.cancelled,
        });
    });

    listeners.swipeUpdate = mouse->events.swipeUpdate.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SSwipeUpdateEvent>(d);

        pointerEvents.swipeUpdate.emit(SSwipeUpdateEvent{
            .timeMs  = E.timeMs,
            .fingers = E.fingers,
            .delta   = E.delta,
        });
    });

    listeners.pinchBegin = mouse->events.pinchBegin.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SPinchBeginEvent>(d);

        pointerEvents.pinchBegin.emit(SPinchBeginEvent{
            .timeMs  = E.timeMs,
            .fingers = E.fingers,
        });
    });

    listeners.pinchEnd = mouse->events.pinchEnd.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SPinchEndEvent>(d);

        pointerEvents.pinchEnd.emit(SPinchEndEvent{
            .timeMs    = E.timeMs,
            .cancelled = E.cancelled,
        });
    });

    listeners.pinchUpdate = mouse->events.pinchUpdate.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SPinchUpdateEvent>(d);

        pointerEvents.pinchUpdate.emit(SPinchUpdateEvent{
            .timeMs   = E.timeMs,
            .fingers  = E.fingers,
            .delta    = E.delta,
            .scale    = E.scale,
            .rotation = E.rotation,
        });
    });

    listeners.holdBegin = mouse->events.holdBegin.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SHoldBeginEvent>(d);

        pointerEvents.holdBegin.emit(SHoldBeginEvent{
            .timeMs  = E.timeMs,
            .fingers = E.fingers,
        });
    });

    listeners.holdEnd = mouse->events.holdEnd.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SHoldEndEvent>(d);

        pointerEvents.holdEnd.emit(SHoldEndEvent{
            .timeMs    = E.timeMs,
            .cancelled = E.cancelled,
        });
    });

    deviceName = mouse->getName();
}

bool CMouse::isVirtual() {
    return false;
}

SP<Aquamarine::IPointer> CMouse::aq() {
    return mouse.lock();
}
