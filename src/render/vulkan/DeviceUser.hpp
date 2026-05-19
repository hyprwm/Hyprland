#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "Device.hpp"

namespace Render::VK {
    class IDeviceUser {
      public:
        IDeviceUser(WP<CHyprVulkanDevice> device);

        VkDevice vkDevice();

      protected:
        WP<CHyprVulkanDevice> m_device;
    };
}