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

    m_listeners.destroy = m_mouse->events.destroy.listen([this] {
        m_mouse.reset();
        m_events.destroy.emit();
    });

    m_listeners.motion = m_mouse->events.move.listen([this](const Aquamarine::IPointer::SMoveEvent& event) {
        m_pointerEvents.motion.emit(SMotionEvent{
            .timeMs  = event.timeMs,
            .delta   = event.delta,
            .unaccel = event.unaccel,
            .mouse   = true,
            .device  = m_self.lock(),
        });
    });

    m_listeners.motionAbsolute = m_mouse->events.warp.listen([this](const Aquamarine::IPointer::SWarpEvent& event) {
        m_pointerEvents.motionAbsolute.emit(SMotionAbsoluteEvent{
            .timeMs   = event.timeMs,
            .absolute = event.absolute,
            .device   = m_self.lock(),
        });
    });

    m_listeners.button = m_mouse->events.button.listen([this](const Aquamarine::IPointer::SButtonEvent& event) {
        m_pointerEvents.button.emit(SButtonEvent{
            .timeMs = event.timeMs,
            .button = event.button,
            .state  = event.pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED,
            .mouse  = true,
        });
    });

    m_listeners.axis = m_mouse->events.axis.listen([this](const Aquamarine::IPointer::SAxisEvent& event) {
        m_pointerEvents.axis.emit(SAxisEvent{
            .timeMs            = event.timeMs,
            .source            = static_cast<wl_pointer_axis_source>(event.source),
            .axis              = static_cast<wl_pointer_axis>(event.axis),
            .relativeDirection = static_cast<wl_pointer_axis_relative_direction>(event.direction),
            .delta             = event.delta,
            .deltaDiscrete     = event.discrete,
            .mouse             = true,
        });
    });

    m_listeners.frame = m_mouse->events.frame.listen([this] { m_pointerEvents.frame.emit(); });

    m_listeners.swipeBegin = m_mouse->events.swipeBegin.listen([this](const Aquamarine::IPointer::SSwipeBeginEvent& event) {
        m_pointerEvents.swipeBegin.emit(SSwipeBeginEvent{
            .timeMs  = event.timeMs,
            .fingers = event.fingers,
        });
    });

    m_listeners.swipeEnd = m_mouse->events.swipeEnd.listen([this](const Aquamarine::IPointer::SSwipeEndEvent& event) {
        m_pointerEvents.swipeEnd.emit(SSwipeEndEvent{
            .timeMs    = event.timeMs,
            .cancelled = event.cancelled,
        });
    });

    m_listeners.swipeUpdate = m_mouse->events.swipeUpdate.listen([this](const Aquamarine::IPointer::SSwipeUpdateEvent& event) {
        m_pointerEvents.swipeUpdate.emit(SSwipeUpdateEvent{
            .timeMs  = event.timeMs,
            .fingers = event.fingers,
            .delta   = event.delta,
        });
    });

    m_listeners.pinchBegin = m_mouse->events.pinchBegin.listen([this](const Aquamarine::IPointer::SPinchBeginEvent& event) {
        m_pointerEvents.pinchBegin.emit(SPinchBeginEvent{
            .timeMs  = event.timeMs,
            .fingers = event.fingers,
        });
    });

    m_listeners.pinchEnd = m_mouse->events.pinchEnd.listen([this](const Aquamarine::IPointer::SPinchEndEvent& event) {
        m_pointerEvents.pinchEnd.emit(SPinchEndEvent{
            .timeMs    = event.timeMs,
            .cancelled = event.cancelled,
        });
    });

    m_listeners.pinchUpdate = m_mouse->events.pinchUpdate.listen([this](const Aquamarine::IPointer::SPinchUpdateEvent& event) {
        m_pointerEvents.pinchUpdate.emit(SPinchUpdateEvent{
            .timeMs   = event.timeMs,
            .fingers  = event.fingers,
            .delta    = event.delta,
            .scale    = event.scale,
            .rotation = event.rotation,
        });
    });

    m_listeners.holdBegin = m_mouse->events.holdBegin.listen([this](const Aquamarine::IPointer::SHoldBeginEvent& event) {
        m_pointerEvents.holdBegin.emit(SHoldBeginEvent{
            .timeMs  = event.timeMs,
            .fingers = event.fingers,
        });
    });

    m_listeners.holdEnd = m_mouse->events.holdEnd.listen([this](const Aquamarine::IPointer::SHoldEndEvent& event) {
        m_pointerEvents.holdEnd.emit(SHoldEndEvent{
            .timeMs    = event.timeMs,
            .cancelled = event.cancelled,
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
