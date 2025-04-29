#include "VirtualPointer.hpp"
#include "../protocols/VirtualPointer.hpp"
#include <aquamarine/input/Input.hpp>

SP<CVirtualPointer> CVirtualPointer::create(SP<CVirtualPointerV1Resource> resource) {
    SP<CVirtualPointer> pPointer = SP<CVirtualPointer>(new CVirtualPointer(resource));

    pPointer->m_self = pPointer;

    return pPointer;
}

CVirtualPointer::CVirtualPointer(SP<CVirtualPointerV1Resource> resource) : m_pointer(resource) {
    if UNLIKELY (!resource->good())
        return;

    m_listeners.destroy = m_pointer->events.destroy.registerListener([this](std::any d) {
        m_pointer.reset();
        m_events.destroy.emit();
    });

    m_listeners.motion         = m_pointer->events.move.registerListener([this](std::any d) {
        auto E   = std::any_cast<SMotionEvent>(d);
        E.device = m_self.lock();
        m_pointerEvents.motion.emit(E);
    });
    m_listeners.motionAbsolute = m_pointer->events.warp.registerListener([this](std::any d) {
        // we need to unpack the event and add our device here because it's required to calculate the position correctly
        auto E   = std::any_cast<SMotionAbsoluteEvent>(d);
        E.device = m_self.lock();
        m_pointerEvents.motionAbsolute.emit(E);
    });
    m_listeners.button         = m_pointer->events.button.registerListener([this](std::any d) { m_pointerEvents.button.emit(d); });
    m_listeners.axis           = m_pointer->events.axis.registerListener([this](std::any d) { m_pointerEvents.axis.emit(d); });
    m_listeners.frame          = m_pointer->events.frame.registerListener([this](std::any d) { m_pointerEvents.frame.emit(); });
    m_listeners.swipeBegin     = m_pointer->events.swipeBegin.registerListener([this](std::any d) { m_pointerEvents.swipeBegin.emit(d); });
    m_listeners.swipeEnd       = m_pointer->events.swipeEnd.registerListener([this](std::any d) { m_pointerEvents.swipeEnd.emit(d); });
    m_listeners.swipeUpdate    = m_pointer->events.swipeUpdate.registerListener([this](std::any d) { m_pointerEvents.swipeUpdate.emit(d); });
    m_listeners.pinchBegin     = m_pointer->events.pinchBegin.registerListener([this](std::any d) { m_pointerEvents.pinchBegin.emit(d); });
    m_listeners.pinchEnd       = m_pointer->events.pinchEnd.registerListener([this](std::any d) { m_pointerEvents.pinchEnd.emit(d); });
    m_listeners.pinchUpdate    = m_pointer->events.pinchUpdate.registerListener([this](std::any d) { m_pointerEvents.pinchUpdate.emit(d); });
    m_listeners.holdBegin      = m_pointer->events.holdBegin.registerListener([this](std::any d) { m_pointerEvents.holdBegin.emit(d); });
    m_listeners.holdEnd        = m_pointer->events.holdEnd.registerListener([this](std::any d) { m_pointerEvents.holdEnd.emit(d); });

    m_boundOutput = resource->boundOutput ? resource->boundOutput->szName : "";

    m_deviceName = m_pointer->name;
}

bool CVirtualPointer::isVirtual() {
    return true;
}

SP<Aquamarine::IPointer> CVirtualPointer::aq() {
    return nullptr;
}
