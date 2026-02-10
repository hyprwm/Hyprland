#pragma once

#include "DeviceUser.hpp"

enum eShaderType : uint8_t {
    SH_VERT,
    SH_FRAG,
};

class CVkShader : public IDeviceUser {
  public:
    CVkShader(WP<CHyprVulkanDevice> device, const std::string& source, eShaderType type = SH_FRAG);
    ~CVkShader();

    VkShaderModule module();

  private:
    VkShaderModule m_module;
};