#pragma once

#include "DeviceUser.hpp"
#include "Shader.hpp"
#include <vulkan/vulkan.h>

class CVkShaders : IDeviceUser {
  public:
    CVkShaders(WP<CHyprVulkanDevice> device);
    // ~CVkShaders();

    SP<CVkShader> m_vert;
    SP<CVkShader> m_frag;
    SP<CVkShader> m_border;
    SP<CVkShader> m_rect;
    SP<CVkShader> m_shadow;
    SP<CVkShader> m_matte;
};
