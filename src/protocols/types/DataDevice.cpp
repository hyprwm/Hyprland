#include "DataDevice.hpp"

boolIataSource::hasDnd() {
    return false;
}

boolIataSource::dndDone() {
    return false;
}

boolIataSource::used() {
    return wasUsed;
}

voidIataSource::markUsed() {
    wasUsed = true;
}

eDataSourceTypeIataSource::type() {
    return DATA_SOURCE_TYPE_WAYLAND;
}

voidIataSource::sendDndFinished() {
    ;
}

uint32_tIataSource::actions() {
    return 7; // all
}

voidIataSource::sendDndDropPerformed() {
    ;
}

voidIataSource::sendDndAction(wl_data_device_manager_dnd_action a) {
    ;
}

voidIataOffer::markDead() {
    ;
}
