#pragma once

#include "devices/IHID.hpp"
#include "helpers/memory/Memory.hpp"
#include <string>

#include "DeviceList.hpp"

namespace Keybinds {

    struct SSubmapArgs {
        CDeviceList device;
    };

    class CSubmap {
      public:
        CSubmap(std::string& name, SSubmapArgs&& args);

        CSubmap(CSubmap&&) noexcept            = default;
        CSubmap& operator=(CSubmap&&) noexcept = default;
        CSubmap(const CSubmap&)                = delete;
        CSubmap&           operator=(CSubmap&) = delete;

        bool               matchesDevice(WP<IHID> device) const;

        std::string_view   name() const;
        const CDeviceList& devices() const;

      private:
        std::string m_name;
        CDeviceList m_devices;
    };

}
