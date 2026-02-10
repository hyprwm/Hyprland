#pragma once

#include "DeviceUser.hpp"

class CVkPipelineLayout : public IDeviceUser {
  public:
    CVkPipelineLayout(WP<CHyprVulkanDevice> device);
    ~CVkPipelineLayout();

    VkPipelineLayout      m_layout;
    VkDescriptorSetLayout m_descriptorSet;
    VkSampler             m_sampler;
};