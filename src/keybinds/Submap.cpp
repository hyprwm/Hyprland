#include "Submap.hpp"

#include <algorithm>

using namespace Keybinds;

CSubmap::CSubmap(std::string& name, SSubmapArgs&& args) : m_name(std::move(name)), m_devices(std::move(args.device)) {};

std::string_view CSubmap::name() const {
    return m_name;
}

const CDeviceList& CSubmap::devices() const {
    return m_devices;
}

bool CSubmap::matchesDevice(WP<IHID> device) const {
    if (!device)
        return !m_devices.inclusive();

    return m_devices.contains(device->m_hlName) || std::ranges::any_of(device->m_deviceTags, [this](const auto& tag) { return m_devices.contains(tag); });
}
