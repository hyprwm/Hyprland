#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/Format.hpp"
#include "DeviceUser.hpp"
#include <aquamarine/buffer/Buffer.hpp>
#include <array>
#include <vulkan/vulkan.h>
#include <drm/drm_fourcc.h>

class CHyprVkFramebuffer : public IDeviceUser {
  public:
    CHyprVkFramebuffer(WP<CHyprVulkanDevice> device, SP<Aquamarine::IBuffer> buffer, VkRenderPass renderPass);
    ~CHyprVkFramebuffer();

    WP<Aquamarine::IBuffer> m_hlBuffer;
    bool                    m_initialized = false;

  private:
    void                          initImage(SVkFormatProps props, Aquamarine::SDMABUFAttrs attrs);
    void                          initImageView(VkFormat format);

    std::array<VkDeviceMemory, 4> m_memory      = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImage                       m_image       = VK_NULL_HANDLE;
    VkImageView                   m_imageView   = VK_NULL_HANDLE;
    VkFramebuffer                 m_framebuffer = VK_NULL_HANDLE;

    friend class CHyprVKRenderer;
};