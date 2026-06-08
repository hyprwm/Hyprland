#pragma once

#include "DeviceUser.hpp"
#include "Shader.hpp"
#include "../ShaderLoader.hpp"
#include <vulkan/vulkan.h>

namespace Render::VK {
    class CVkShaders : IDeviceUser {
      public:
        CVkShaders(WP<CHyprVulkanDevice> device, const std::string& shaderPath = "");
        // ~CVkShaders();

        SP<CVkShader> m_vert;
        WP<CVkShader> getShaderVariant(ePreparedFragmentShader frag, uint8_t features = 0);

      private:
        std::array<std::map<uint8_t, SP<CVkShader>>, SH_FRAG_LAST> m_fragVariants;
    };
}