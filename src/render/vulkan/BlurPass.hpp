#pragma once

#include "DeviceUser.hpp"
#include "Shaders.hpp"
#include "Pipeline.hpp"
#include "../Texture.hpp"
#include "../Framebuffer.hpp"

class CVKBlurPass : IDeviceUser {
  public:
    CVKBlurPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders, int passes = 1);
    ~CVKBlurPass();

    DRMFormat    format();
    VkRenderPass vk();
    int          passes();

    SP<ITexture> blurTexture(SP<ITexture> texture, SP<IFramebuffer> first, SP<IFramebuffer> second);

  private:
    std::vector<SP<CVkPipeline>> m_pipelines;

    DRMFormat                    m_drmFormat;
    SP<CVkShaders>               m_shaders;
    int                          m_passes = 1;
    VkRenderPass                 m_vkRenderPass;
};