#include "DeviceList.hpp"

using namespace Keybinds;

CDeviceList::CDeviceList(bool inclusive, DeviceList&& devices) : m_inclusive(inclusive), m_devices(std::move(devices)) {};

bool CDeviceList::inclusive() const {
    return m_inclusive;
}

const DeviceList& CDeviceList::devices() const {
    return m_devices;
};

void CDeviceList::setInclusive(bool value) {
    m_inclusive = value;
};

void CDeviceList::add(std::string& device) {
    m_devices.emplace(std::move(device));
}

bool CDeviceList::contains(const std::string& device) const {
    const bool LISTED = m_devices.contains(device);

    return LISTED == m_inclusive;
};
