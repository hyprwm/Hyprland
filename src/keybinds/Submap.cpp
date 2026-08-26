#include "Submap.hpp"
#include <algorithm>

using namespace Keybinds;

CSubmap::CSubmap(std::string& name, SSubmapArgs metadata) : m_name(std::move(name)), m_devices(std::move(metadata.devices)) {};

std::string_view CSubmap::name() const {
    return m_name;
}

const std::unordered_set<std::string>& CSubmap::devices() const {
    return m_devices;
}

bool CSubmap::matchesDevice(SP<IHID> device) {
    if (m_devices.empty() || !device) {
        return true;
    }

    return m_devices.contains(device->m_hlName) || std::ranges::any_of(device->m_deviceTags, [this](const auto& tag) { return m_devices.contains(tag); });
}
