#pragma once

#include "DeviceUser.hpp"

class CVkPipelineLayout : public IDeviceUser {
  public:
    using KEY = std::tuple<uint32_t, uint32_t, VkFilter>;

    CVkPipelineLayout(WP<CHyprVulkanDevice> device, KEY key);
    CVkPipelineLayout(WP<CHyprVulkanDevice> device, uint32_t vertSize, uint32_t fragSize, VkFilter filter = VK_FILTER_LINEAR) :
        CVkPipelineLayout(device, {vertSize, fragSize, filter}) {};
    ~CVkPipelineLayout();

    KEY                   key();
    VkPipelineLayout      vk();
    VkDescriptorSetLayout descriptorSet();

  private:
    KEY                   m_key;

    VkSampler             m_sampler;
    VkPipelineLayout      m_layout;
    VkDescriptorSetLayout m_descriptorSet;
};