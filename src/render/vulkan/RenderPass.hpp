#pragma once

#include "../../helpers/Format.hpp"
#include "DeviceUser.hpp"
#include <vulkan/vulkan_core.h>

class CVkRenderPass : IDeviceUser {
  public:
    CVkRenderPass(WP<CHyprVulkanDevice> device, DRMFormat format);
    ~CVkRenderPass();

    DRMFormat    m_drmFormat;
    VkRenderPass m_vkRenderPass;
};