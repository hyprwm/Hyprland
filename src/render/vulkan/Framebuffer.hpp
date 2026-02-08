#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/Format.hpp"
#include <aquamarine/buffer/Buffer.hpp>
#include <vulkan/vulkan.h>

class CHyprVkFramebuffer {
  public:
    CHyprVkFramebuffer(SP<Aquamarine::IBuffer> buffer, DRMFormat fmt);
    ~CHyprVkFramebuffer();

    WP<Aquamarine::IBuffer> m_hlBuffer;

  private:
    VkImage       m_image;
    VkImageView   m_imageView;
    VkFramebuffer m_framebuffer;

    DRMFormat     m_drmFormat;

    friend class CHyprVKRenderer;
};