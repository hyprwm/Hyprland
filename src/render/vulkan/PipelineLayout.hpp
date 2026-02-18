#pragma once

#include "DeviceUser.hpp"

class CVkPipelineLayout : public IDeviceUser {
  public:
    using KEY = std::tuple<uint32_t, uint32_t, VkFilter, uint8_t>;

    CVkPipelineLayout(WP<CHyprVulkanDevice> device, KEY key);
    CVkPipelineLayout(WP<CHyprVulkanDevice> device, uint32_t vertSize, uint32_t fragSize, VkFilter filter = VK_FILTER_LINEAR, uint8_t texCount = 1) :
        CVkPipelineLayout(device, {vertSize, fragSize, filter, texCount}) {};
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