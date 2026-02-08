#pragma once
#include <vulkan/vulkan.h>

class CHyprVkFramebuffer {
  public:
    CHyprVkFramebuffer();
    ~CHyprVkFramebuffer();

  private:
    VkImageView   m_imageView;
    VkFramebuffer m_framebuffer;
};