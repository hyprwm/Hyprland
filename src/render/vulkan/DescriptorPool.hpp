#pragma once

#include "DeviceUser.hpp"
#include <vulkan/vulkan_core.h>

class CVKDescriptorPool : IDeviceUser {
  public:
    CVKDescriptorPool(WP<CHyprVulkanDevice> device, VkDescriptorType type, size_t size);
    ~CVKDescriptorPool();

    VkResult         allocateSet(VkDescriptorSetLayout layout, VkDescriptorSet* ds);
    VkDescriptorPool vkPool();

  private:
    VkDescriptorPool m_pool;
    uint32_t         m_available = 0;
};