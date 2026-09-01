#pragma once

#include <string>
#include <unordered_set>

namespace Keybinds {

    using DeviceList = std::unordered_set<std::string>;

    class CDeviceList {
      public:
        CDeviceList(bool inclusive, DeviceList&& devices);
        CDeviceList() = default;

        CDeviceList(CDeviceList&&) noexcept             = default;
        CDeviceList& operator=(CDeviceList&&) noexcept  = default;
        CDeviceList(CDeviceList&)                       = delete;
        CDeviceList&      operator=(const CDeviceList&) = delete;

        void              setInclusive(bool value);
        void              add(std::string& device);
        bool              contains(const std::string& device) const;

        bool              inclusive() const;
        const DeviceList& devices() const;

      private:
        bool       m_inclusive = false;
        DeviceList m_devices;
    };
}
