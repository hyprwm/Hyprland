#pragma once

#include "../../helpers/Format.hpp"
#include "DeviceUser.hpp"
#include "render/ShaderLoader.hpp"
#include "render/vulkan/Pipeline.hpp"
#include "render/vulkan/Shaders.hpp"
#include <vulkan/vulkan_core.h>

namespace Render::VK {
    class CVkPipeline;

    class CVkRenderPass : IDeviceUser {
      public:
        CVkRenderPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders);
        ~CVkRenderPass();

        const DRMFormat m_drmFormat;
        VkRenderPass    m_vkRenderPass;

        WP<CVkPipeline> borderPipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> rectPipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> shadowPipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> texturePipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> textureMattePipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);
        WP<CVkPipeline> passPipeline(uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING);

      private:
        SP<CVkPipeline> getPipeline(ePreparedFragmentShader frag, uint8_t features = SH_FEAT_RGBA | SH_FEAT_ROUNDING, int texCount = 1, int uboCount = 0);
        std::array<std::map<uint8_t, SP<CVkPipeline>>, SH_FRAG_LAST> m_pipelines;

        SP<CVkShaders>                                               m_shaders;
    };
}