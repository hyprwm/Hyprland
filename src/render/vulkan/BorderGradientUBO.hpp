#pragma once

#include "DescriptorPool.hpp"
#include "render/vulkan/DeviceUser.hpp"
#include <vulkan/vulkan.h>

class CVKBorderGradientUBO : IDeviceUser {
  public:
    CVKBorderGradientUBO(WP<CHyprVulkanDevice> device, VkDescriptorSetLayout layout);
    ~CVKBorderGradientUBO();

    void            update(const SVkBorderGradientShaderData& gradients);
    VkDescriptorSet ds();

  private:
    VkDescriptorSet       m_desctiptorSet;
    WP<CVKDescriptorPool> m_dsPool;
    VkBuffer              m_buffer;
    VkDeviceMemory        m_bufferMemory;
};
