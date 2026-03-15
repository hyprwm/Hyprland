#pragma once

#include "DeviceUser.hpp"
#include "Shaders.hpp"
#include "Pipeline.hpp"
#include "../Texture.hpp"
#include "../Framebuffer.hpp"

namespace Render::VK {
    class CVKBlurPass : IDeviceUser {
      public:
        CVKBlurPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders, int passes = 1);
        ~CVKBlurPass();

        DRMFormat    format();
        VkRenderPass vk();
        int          passes();

        SP<ITexture> blurTexture(SP<ITexture> texture, SP<IFramebuffer> first, SP<IFramebuffer> second, float a, const CRegion& damage);

      private:
        SP<CVkPipeline> m_preparePipeline;
        SP<CVkPipeline> m_prepareCMPipeline;
        SP<CVkPipeline> m_blur1Pipeline;
        SP<CVkPipeline> m_blur2Pipeline;
        SP<CVkPipeline> m_finishPipeline;
        SP<CVkPipeline> m_finishCMPipeline;

        DRMFormat       m_drmFormat;
        SP<CVkShaders>  m_shaders;
        int             m_passes = 1;
    };
}