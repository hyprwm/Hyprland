#include "DataDevice.hpp"

bool CIDataSource::hasDnd() {
    return false;
}

bool CIDataSource::dndDone() {
    return false;
}

bool CIDataSource::used() {
    return wasUsed;
}

void CIDataSource::markUsed() {
    wasUsed = true;
}

eDataSourceType CIDataSource::type() {
    return DATA_SOURCE_TYPE_WAYLAND;
}

void CIDataSource::sendDndFinished() {
    ;
}

uint32_t CIDataSource::actions() {
    return 7; // all
}

void CIDataSource::sendDndDropPerformed() {
    ;
}

void CIDataSource::sendDndAction(wl_data_device_manager_dnd_action a) {
    ;
}

void CIDataOffer::markDead() {
    ;
}
