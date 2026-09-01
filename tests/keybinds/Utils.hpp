#pragma once

#include <keybinds/Submap.hpp>

inline Keybinds::CSubmap makeSubmap(std::string name, std::unordered_set<std::string>&& devices, const bool inclusive) {
    Keybinds::SSubmapArgs args{.device = Keybinds::CDeviceList(inclusive, std::move(devices))};
    return Keybinds::CSubmap(name, std::move(args));
};
