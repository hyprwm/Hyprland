#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "Device.hpp"

class IDeviceUser {
  public:
    IDeviceUser(WP<CHyprVulkanDevice> device);

    VkDevice vkDevice();

  protected:
    WP<CHyprVulkanDevice> m_device;
};
