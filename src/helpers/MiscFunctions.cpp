#include "MiscFunctions.hpp"
#include "../defines.hpp"

void addWLSignal(wl_signal* pSignal, wl_listener* pListener, void* pOwner, std::string ownerString) {
    ASSERT(pSignal);
    ASSERT(pListener);
    
    wl_signal_add(pSignal, pListener);

    Debug::log(LOG, "Registered signal for owner %x: %x -> %x (owner: %s)", pOwner, pSignal, pListener, ownerString.c_str());
}