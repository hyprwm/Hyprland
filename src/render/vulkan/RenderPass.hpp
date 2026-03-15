#pragma once

#include "../../helpers/Format.hpp"
#include "DeviceUser.hpp"
#include "../ShaderLoader.hpp"
#include "Pipeline.hpp"
#include "Shaders.hpp"
#include <vulkan/vulkan_core.h>

namespace Render::VK {
    class CVkPipeline;
    class CHyprVkCommandBuffer;
    class CVKFramebuffer;

    class CVkRenderPass : IDeviceUser {
      public:
        CVkRenderPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders);
        ~CVkRenderPass();

        const DRMFormat m_drmFormat;
        VkFormat        m_vkFormat;

        WP<CVkPipeline> borderPipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> rectPipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> shadowPipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> texturePipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> textureMattePipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> passPipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);

        void            beginRendering(SP<CHyprVkCommandBuffer> cb, SP<CVKFramebuffer> target);
        void            endRendering();

      private:
        SP<CVkPipeline> getPipeline(ePreparedFragmentShader frag, uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING, int texCount = 1, int uboCount = 0);
        std::array<std::map<uint8_t, SP<CVkPipeline>>, SH_FRAG_LAST> m_pipelines;

        SP<CVkShaders>                                               m_shaders;
        SP<CHyprVkCommandBuffer>                                     m_cmdBuffer;
        SP<CVKFramebuffer>                                           m_targetFB;
    };
}