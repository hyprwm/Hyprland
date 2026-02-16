#pragma once

#include "../../helpers/Format.hpp"
#include "DeviceUser.hpp"
#include "render/vulkan/Pipeline.hpp"
#include "render/vulkan/Shaders.hpp"
#include <vulkan/vulkan_core.h>

class CVkPipeline;

class CVkRenderPass : IDeviceUser {
  public:
    CVkRenderPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders);
    ~CVkRenderPass();

    DRMFormat       m_drmFormat;
    VkRenderPass    m_vkRenderPass;

    WP<CVkPipeline> borderPipeline();
    WP<CVkPipeline> rectPipeline();
    WP<CVkPipeline> texturePipeline();

  private:
    SP<CVkPipeline> m_borderPipeline;
    SP<CVkPipeline> m_rectPipeline;
    SP<CVkPipeline> m_texturePipeline;

    SP<CVkShaders>  m_shaders;
};
