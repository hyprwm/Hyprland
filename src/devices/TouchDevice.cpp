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

    m_listeners.destroy = m_touch->events.destroy.listen([this] {
        m_events.destroy.emit();
        m_touch.reset();
    });

    m_listeners.down = m_touch->events.down.listen([this](const Aquamarine::ITouch::SDownEvent& event) {
        m_touchEvents.down.emit(SDownEvent{
            .timeMs  = event.timeMs,
            .touchID = event.touchID,
            .pos     = event.pos,
            .device  = m_self.lock(),
        });
    });

    m_listeners.up = m_touch->events.up.listen([this](const Aquamarine::ITouch::SUpEvent& event) {
        m_touchEvents.up.emit(SUpEvent{
            .timeMs  = event.timeMs,
            .touchID = event.touchID,
        });
    });

    m_listeners.motion = m_touch->events.move.listen([this](const Aquamarine::ITouch::SMotionEvent& event) {
        m_touchEvents.motion.emit(SMotionEvent{
            .timeMs  = event.timeMs,
            .touchID = event.touchID,
            .pos     = event.pos,
        });
    });

    m_listeners.cancel = m_touch->events.cancel.listen([this](const Aquamarine::ITouch::SCancelEvent& event) {
        m_touchEvents.cancel.emit(SCancelEvent{
            .timeMs  = event.timeMs,
            .touchID = event.touchID,
        });
    });

    m_listeners.frame = m_touch->events.frame.listen([this] { m_touchEvents.frame.emit(); });

    m_deviceName = m_touch->getName();
}

bool CTouchDevice::isVirtual() {
    return false;
}

SP<Aquamarine::ITouch> CTouchDevice::aq() {
    return m_touch.lock();
}
