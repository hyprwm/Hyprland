#include "TouchDevice.hpp"
#include "../defines.hpp"
#include <aquamarine/input/Input.hpp>

SP<CTouchDevice> CTouchDevice::create(SP<Aquamarine::ITouch> touch) {
    SP<CTouchDevice> pTouch = SP<CTouchDevice>(new CTouchDevice(touch));

    pTouch->self = pTouch;

    return pTouch;
}

CTouchDevice::CTouchDevice(SP<Aquamarine::ITouch> touch_) : touch(touch_) {
    if (!touch)
        return;

    m_listeners.destroy = touch->events.destroy.registerListener([this](std::any d) {
        events.destroy.emit();
        touch.reset();
    });

    m_listeners.down = touch->events.down.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITouch::SDownEvent>(d);

        touchEvents.down.emit(SDownEvent{
            .timeMs  = E.timeMs,
            .touchID = E.touchID,
            .pos     = E.pos,
            .device  = self.lock(),
        });
    });

    m_listeners.up = touch->events.up.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITouch::SUpEvent>(d);

        touchEvents.up.emit(SUpEvent{
            .timeMs  = E.timeMs,
            .touchID = E.touchID,
        });
    });

    m_listeners.motion = touch->events.move.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITouch::SMotionEvent>(d);

        touchEvents.motion.emit(SMotionEvent{
            .timeMs  = E.timeMs,
            .touchID = E.touchID,
            .pos     = E.pos,
        });
    });

    m_listeners.cancel = touch->events.cancel.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::ITouch::SCancelEvent>(d);

        touchEvents.cancel.emit(SCancelEvent{
            .timeMs  = E.timeMs,
            .touchID = E.touchID,
        });
    });

    m_listeners.frame = touch->events.frame.registerListener([this](std::any d) { touchEvents.frame.emit(); });

    deviceName = touch->getName();
}

bool CTouchDevice::isVirtual() {
    return false;
}

SP<Aquamarine::ITouch> CTouchDevice::aq() {
    return touch.lock();
}
