#pragma once

#include "DeviceUser.hpp"
#include "PipelineLayout.hpp"
#include "RenderPass.hpp"

class CVkRenderPass;
class CVkShader;

class CVkPipeline : public IDeviceUser {
  public:
    using KEY = std::tuple<VkShaderModule, VkShaderModule>;
    CVkPipeline(WP<CHyprVulkanDevice> device, VkRenderPass renderPass, WP<CVkShader> vert, WP<CVkShader> frag, uint8_t texCount = 1);
    ~CVkPipeline();

    VkPipeline            vk();
    WP<CVkPipelineLayout> layout();
    KEY                   key();

  private:
    WP<CVkPipelineLayout> m_layout;
    VkPipeline            m_vkPipeline;
    KEY                   m_key;
};