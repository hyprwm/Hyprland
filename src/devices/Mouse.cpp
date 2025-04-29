#include "Mouse.hpp"
#include "../defines.hpp"
#include <aquamarine/input/Input.hpp>

SP<CMouse> CMouse::create(SP<Aquamarine::IPointer> mouse) {
    SP<CMouse> pMouse = SP<CMouse>(new CMouse(mouse));

    pMouse->m_self = pMouse;

    return pMouse;
}

CMouse::CMouse(SP<Aquamarine::IPointer> mouse_) : m_mouse(mouse_) {
    if (!m_mouse)
        return;

    if (auto handle = m_mouse->getLibinputHandle()) {
        double w = 0, h = 0;
        m_isTouchpad = libinput_device_has_capability(handle, LIBINPUT_DEVICE_CAP_POINTER) && libinput_device_get_size(handle, &w, &h) == 0;
    }

    m_listeners.destroy = m_mouse->events.destroy.registerListener([this](std::any d) {
        m_mouse.reset();
        m_events.destroy.emit();
    });

    m_listeners.motion = m_mouse->events.move.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SMoveEvent>(d);

        m_pointerEvents.motion.emit(SMotionEvent{
            .timeMs  = E.timeMs,
            .delta   = E.delta,
            .unaccel = E.unaccel,
            .mouse   = true,
            .device  = m_self.lock(),
        });
    });

    m_listeners.motionAbsolute = m_mouse->events.warp.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SWarpEvent>(d);

        m_pointerEvents.motionAbsolute.emit(SMotionAbsoluteEvent{
            .timeMs   = E.timeMs,
            .absolute = E.absolute,
            .device   = m_self.lock(),
        });
    });

    m_listeners.button = m_mouse->events.button.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SButtonEvent>(d);

        m_pointerEvents.button.emit(SButtonEvent{
            .timeMs = E.timeMs,
            .button = E.button,
            .state  = E.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED,
            .mouse  = true,
        });
    });

    m_listeners.axis = m_mouse->events.axis.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SAxisEvent>(d);

        m_pointerEvents.axis.emit(SAxisEvent{
            .timeMs            = E.timeMs,
            .source            = (wl_pointer_axis_source)E.source,
            .axis              = (wl_pointer_axis)E.axis,
            .relativeDirection = (wl_pointer_axis_relative_direction)E.direction,
            .delta             = E.delta,
            .deltaDiscrete     = E.discrete,
            .mouse             = true,
        });
    });

    m_listeners.frame = m_mouse->events.frame.registerListener([this](std::any d) { m_pointerEvents.frame.emit(); });

    m_listeners.swipeBegin = m_mouse->events.swipeBegin.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SSwipeBeginEvent>(d);

        m_pointerEvents.swipeBegin.emit(SSwipeBeginEvent{
            .timeMs  = E.timeMs,
            .fingers = E.fingers,
        });
    });

    m_listeners.swipeEnd = m_mouse->events.swipeEnd.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SSwipeEndEvent>(d);

        m_pointerEvents.swipeEnd.emit(SSwipeEndEvent{
            .timeMs    = E.timeMs,
            .cancelled = E.cancelled,
        });
    });

    m_listeners.swipeUpdate = m_mouse->events.swipeUpdate.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SSwipeUpdateEvent>(d);

        m_pointerEvents.swipeUpdate.emit(SSwipeUpdateEvent{
            .timeMs  = E.timeMs,
            .fingers = E.fingers,
            .delta   = E.delta,
        });
    });

    m_listeners.pinchBegin = m_mouse->events.pinchBegin.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SPinchBeginEvent>(d);

        m_pointerEvents.pinchBegin.emit(SPinchBeginEvent{
            .timeMs  = E.timeMs,
            .fingers = E.fingers,
        });
    });

    m_listeners.pinchEnd = m_mouse->events.pinchEnd.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SPinchEndEvent>(d);

        m_pointerEvents.pinchEnd.emit(SPinchEndEvent{
            .timeMs    = E.timeMs,
            .cancelled = E.cancelled,
        });
    });

    m_listeners.pinchUpdate = m_mouse->events.pinchUpdate.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SPinchUpdateEvent>(d);

        m_pointerEvents.pinchUpdate.emit(SPinchUpdateEvent{
            .timeMs   = E.timeMs,
            .fingers  = E.fingers,
            .delta    = E.delta,
            .scale    = E.scale,
            .rotation = E.rotation,
        });
    });

    m_listeners.holdBegin = m_mouse->events.holdBegin.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SHoldBeginEvent>(d);

        m_pointerEvents.holdBegin.emit(SHoldBeginEvent{
            .timeMs  = E.timeMs,
            .fingers = E.fingers,
        });
    });

    m_listeners.holdEnd = m_mouse->events.holdEnd.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IPointer::SHoldEndEvent>(d);

        m_pointerEvents.holdEnd.emit(SHoldEndEvent{
            .timeMs    = E.timeMs,
            .cancelled = E.cancelled,
        });
    });

    m_deviceName = m_mouse->getName();
}

bool CMouse::isVirtual() {
    return false;
}

SP<Aquamarine::IPointer> CMouse::aq() {
    return m_mouse.lock();
}
