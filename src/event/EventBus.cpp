#include "EventBus.hpp"

using namespace Event;

UP<CEventBus>& Event::bus() {
    static UP<CEventBus> p = makeUnique<CEventBus>();
    return p;
}
