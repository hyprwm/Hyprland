#pragma once

#include "DeviceUser.hpp"
#include "PipelineLayout.hpp"
#include "RenderPass.hpp"
#include "Shaders.hpp"

class CVkPipeline : public IDeviceUser {
  public:
    CVkPipeline(WP<CHyprVulkanDevice> device, WP<CVkRenderPass> renderPass, WP<CVkPipelineLayout> layout, WP<CVkShaders> shaders);
    ~CVkPipeline();

  private:
    WP<CVkRenderPass>     m_renderPass;
    WP<CVkPipelineLayout> m_layout;
    VkPipeline            m_vkPipeline;
};