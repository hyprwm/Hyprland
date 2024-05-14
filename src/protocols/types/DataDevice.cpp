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
