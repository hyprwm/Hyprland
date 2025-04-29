#include "TouchDevice.hpp"
#include "../defines.hpp"
#include <aquamarine/input/Input.hpp>

SP<CTouchDevice> CTouchDevice::create(SP<Aquamarine::ITouch> touch) {
    SP<CTouchDevice> pTouch = SP<CTouchDevice>(new CTouchDevice(touch));

    pTouch->m_self = pTouch;

    return pTouch;
}

CTouchDevice::CTouchDevice(SP<Aquamarine::ITouch> touch_) : m_touch(touch_) {
    if (!m_touch)
        return;

    m_listeners.destroy = m_touch->events.destroy.registerListener([this](std::any d) {
        m_events.destroy.emit();
        m_touch.reset();
    });

    m_listeners.down = m_touch->events.down.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITouch::SDownEvent>(d);

        m_touchEvents.down.emit(SDownEvent{
            .timeMs  = E.timeMs,
            .touchID = E.touchID,
            .pos     = E.pos,
            .device  = m_self.lock(),
        });
    });

    m_listeners.up = m_touch->events.up.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITouch::SUpEvent>(d);

        m_touchEvents.up.emit(SUpEvent{
            .timeMs  = E.timeMs,
            .touchID = E.touchID,
        });
    });

    m_listeners.motion = m_touch->events.move.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITouch::SMotionEvent>(d);

        m_touchEvents.motion.emit(SMotionEvent{
            .timeMs  = E.timeMs,
            .touchID = E.touchID,
            .pos     = E.pos,
        });
    });

    m_listeners.cancel = m_touch->events.cancel.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITouch::SCancelEvent>(d);

        m_touchEvents.cancel.emit(SCancelEvent{
            .timeMs  = E.timeMs,
            .touchID = E.touchID,
        });
    });

    m_listeners.frame = m_touch->events.frame.registerListener([this](std::any d) { m_touchEvents.frame.emit(); });

    m_deviceName = m_touch->getName();
}

bool CTouchDevice::isVirtual() {
    return false;
}

SP<Aquamarine::ITouch> CTouchDevice::aq() {
    return m_touch.lock();
}
