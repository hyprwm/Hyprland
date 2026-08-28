#include "Submap.hpp"
#include <algorithm>

using namespace Keybinds;

CSubmap::CSubmap(std::string& name, SSubmapArgs args) : m_name(std::move(name)), m_devices(std::move(args.devices)), m_inclusive(args.inclusive) {};

std::string_view CSubmap::name() const {
    return m_name;
}

bool CSubmap::inclusive() const {
    return m_inclusive;
}

const std::unordered_set<std::string>& CSubmap::devices() const {
    return m_devices;
}

bool CSubmap::matchesDevice(SP<IHID> device) const {
    if (!device)
        return !m_inclusive;

    const bool LISTED = m_devices.contains(device->m_hlName) || std::ranges::any_of(device->m_deviceTags, [this](const auto& tag) { return m_devices.contains(tag); });

    return m_inclusive ? LISTED : !LISTED;
}
