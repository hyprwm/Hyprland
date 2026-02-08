#pragma once

#include <vulkan/vulkan_core.h>

class CVkRenderPass {
  public:
    CVkRenderPass();
    ~CVkRenderPass();

    VkRenderPass m_vkRenderPass;
};