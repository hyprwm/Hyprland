#pragma once

#include "DeviceUser.hpp"

enum eShaderType : uint8_t {
    SH_VERT,
    SH_FRAG,
};

class CVkShader : public IDeviceUser {
  public:
    CVkShader(WP<CHyprVulkanDevice> device, const std::string& source, uint32_t pushSize, eShaderType type = SH_FRAG);
    ~CVkShader();

    VkShaderModule module();
    uint32_t       pushSize();

  private:
    VkShaderModule m_module;
    uint32_t       m_pushSize;
};