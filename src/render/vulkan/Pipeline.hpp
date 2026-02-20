#pragma once

#include "DeviceUser.hpp"
#include "PipelineLayout.hpp"
#include "RenderPass.hpp"

class CVkRenderPass;
class CVkShader;

class CVkPipeline : public IDeviceUser {
  public:
    using KEY = std::tuple<VkShaderModule, VkShaderModule>;
    struct SSettings {
        uint8_t texCount = 1;
        uint8_t subpass  = 0;
        bool    blend    = true;
        uint8_t uboCount = 0;
    };
    CVkPipeline(WP<CHyprVulkanDevice> device, VkRenderPass renderPass, WP<CVkShader> vert, WP<CVkShader> frag,
                const SSettings& settings = {.texCount = 1, .subpass = 0, .blend = true, .uboCount = 0});
    ~CVkPipeline();

    VkPipeline            vk();
    WP<CVkPipelineLayout> layout();
    KEY                   key();

  private:
    WP<CVkPipelineLayout> m_layout;
    VkPipeline            m_vkPipeline;
    KEY                   m_key;
};