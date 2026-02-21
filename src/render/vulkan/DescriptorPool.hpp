#pragma once

#include "DeviceUser.hpp"
#include <vulkan/vulkan_core.h>

class CVKDescriptorPool : IDeviceUser {
  public:
    enum eDSPoolType : uint8_t {
        DSP_SAMPLER = 0,
        DSP_UBO     = 1,
    };

    CVKDescriptorPool(WP<CHyprVulkanDevice> device, VkDescriptorType type, size_t size);
    ~CVKDescriptorPool();

    VkResult         allocateSet(VkDescriptorSetLayout layout, VkDescriptorSet* ds);
    VkDescriptorPool vkPool();

  private:
    VkDescriptorPool m_pool;
    uint32_t         m_available = 0;
};