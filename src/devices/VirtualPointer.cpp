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

    m_listeners.destroy = m_pointer->m_events.destroy.listen([this] {
        m_pointer.reset();
        m_events.destroy.emit();
    });

    m_listeners.motion         = m_pointer->m_events.move.listen([this](SMotionEvent event) {
        event.device = m_self.lock();
        m_pointerEvents.motion.emit(event);
    });
    m_listeners.motionAbsolute = m_pointer->m_events.warp.listen([this](SMotionAbsoluteEvent event) {
        // we need to unpack the event and add our device here because it's required to calculate the position correctly
        event.device = m_self.lock();
        m_pointerEvents.motionAbsolute.emit(event);
    });
    m_listeners.button         = m_pointer->m_events.button.forward(m_pointerEvents.button);
    m_listeners.axis           = m_pointer->m_events.axis.forward(m_pointerEvents.axis);
    m_listeners.frame          = m_pointer->m_events.frame.forward(m_pointerEvents.frame);
    m_listeners.swipeBegin     = m_pointer->m_events.swipeBegin.forward(m_pointerEvents.swipeBegin);
    m_listeners.swipeEnd       = m_pointer->m_events.swipeEnd.forward(m_pointerEvents.swipeEnd);
    m_listeners.swipeUpdate    = m_pointer->m_events.swipeUpdate.forward(m_pointerEvents.swipeUpdate);
    m_listeners.pinchBegin     = m_pointer->m_events.pinchBegin.forward(m_pointerEvents.pinchBegin);
    m_listeners.pinchEnd       = m_pointer->m_events.pinchEnd.forward(m_pointerEvents.pinchEnd);
    m_listeners.pinchUpdate    = m_pointer->m_events.pinchUpdate.forward(m_pointerEvents.pinchUpdate);
    m_listeners.holdBegin      = m_pointer->m_events.holdBegin.forward(m_pointerEvents.holdBegin);
    m_listeners.holdEnd        = m_pointer->m_events.holdEnd.forward(m_pointerEvents.holdEnd);

    m_boundOutput = resource->m_boundOutput ? resource->m_boundOutput->m_name : "";

    m_deviceName = m_pointer->m_name;
}

bool CVirtualPointer::isVirtual() {
    return true;
}

SP<Aquamarine::IPointer> CVirtualPointer::aq() {
    return nullptr;
}
