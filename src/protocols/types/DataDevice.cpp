#include "DataDevice.hpp"

bool IDataSource::hasDnd() {
    return false;
}

bool IDataSource::dndDone() {
    return false;
}

bool IDataSource::used() {
    return wasUsed;
}

void IDataSource::markUsed() {
    wasUsed = true;
}

eDataSourceType IDataSource::type() {
    return DATA_SOURCE_TYPE_WAYLAND;
}

void IDataSource::sendDndFinished() {
    ;
}

uint32_t IDataSource::actions() {
    return 7; // all
}

void IDataSource::sendDndDropPerformed() {
    ;
}

void IDataSource::sendDndAction(wl_data_device_manager_dnd_action a) {
    ;
}

void IDataOffer::markDead() {
    ;
}
