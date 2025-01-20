#include "VirtualPointer.hpp"
#include "../protocols/VirtualPointer.hpp"
#include <aquamarine/input/Input.hpp>

SP<CVirtualPointer> CVirtualPointer::create(SP<CVirtualPointerV1Resource> resource) {
    SP<CVirtualPointer> pPointer = SP<CVirtualPointer>(new CVirtualPointer(resource));

    pPointer->self = pPointer;

    return pPointer;
}

CVirtualPointer::CVirtualPointer(SP<CVirtualPointerV1Resource> resource) : pointer(resource) {
    if UNLIKELY (!resource->good())
        return;

    listeners.destroy = pointer->events.destroy.registerListener([this](std::any d) {
        pointer.reset();
        events.destroy.emit();
    });

    listeners.motion         = pointer->events.move.registerListener([this](std::any d) { pointerEvents.motion.emit(d); });
    listeners.motionAbsolute = pointer->events.warp.registerListener([this](std::any d) {
        // we need to unpack the event and add our device here because it's required to calculate the position correctly
        auto E   = std::any_cast<SMotionAbsoluteEvent>(d);
        E.device = self.lock();
        pointerEvents.motionAbsolute.emit(E);
    });
    listeners.button         = pointer->events.button.registerListener([this](std::any d) { pointerEvents.button.emit(d); });
    listeners.axis           = pointer->events.axis.registerListener([this](std::any d) { pointerEvents.axis.emit(d); });
    listeners.frame          = pointer->events.frame.registerListener([this](std::any d) { pointerEvents.frame.emit(); });
    listeners.swipeBegin     = pointer->events.swipeBegin.registerListener([this](std::any d) { pointerEvents.swipeBegin.emit(d); });
    listeners.swipeEnd       = pointer->events.swipeEnd.registerListener([this](std::any d) { pointerEvents.swipeEnd.emit(d); });
    listeners.swipeUpdate    = pointer->events.swipeUpdate.registerListener([this](std::any d) { pointerEvents.swipeUpdate.emit(d); });
    listeners.pinchBegin     = pointer->events.pinchBegin.registerListener([this](std::any d) { pointerEvents.pinchBegin.emit(d); });
    listeners.pinchEnd       = pointer->events.pinchEnd.registerListener([this](std::any d) { pointerEvents.pinchEnd.emit(d); });
    listeners.pinchUpdate    = pointer->events.pinchUpdate.registerListener([this](std::any d) { pointerEvents.pinchUpdate.emit(d); });
    listeners.holdBegin      = pointer->events.holdBegin.registerListener([this](std::any d) { pointerEvents.holdBegin.emit(d); });
    listeners.holdEnd        = pointer->events.holdEnd.registerListener([this](std::any d) { pointerEvents.holdEnd.emit(d); });

    boundOutput = resource->boundOutput ? resource->boundOutput->szName : "";

    deviceName = pointer->name;
}

bool CVirtualPointer::isVirtual() {
    return true;
}

SP<Aquamarine::IPointer> CVirtualPointer::aq() {
    return nullptr;
}
