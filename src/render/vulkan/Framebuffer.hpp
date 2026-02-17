#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/Format.hpp"
#include "DeviceUser.hpp"
#include "../Framebuffer.hpp"
#include "render/Renderbuffer.hpp"
#include "render/Texture.hpp"
#include "render/vulkan/VKTexture.hpp"
#include <aquamarine/buffer/Buffer.hpp>
#include <array>
#include <vulkan/vulkan.h>
#include <drm/drm_fourcc.h>

class CVKTexture;

class CHyprVkFramebuffer : public IDeviceUser {
  public:
    CHyprVkFramebuffer(WP<CHyprVulkanDevice> device, VkRenderPass renderPass, int w, int h, uint32_t format);
    CHyprVkFramebuffer(WP<CHyprVulkanDevice> device, SP<Aquamarine::IBuffer> buffer, VkRenderPass renderPass);
    ~CHyprVkFramebuffer();

    bool           m_initialized = false;

    VkFramebuffer  vk();
    VkImage        vkImage();
    SP<CVKTexture> texture();

  private:
    void           initImage(SVkFormatProps props, int w, int h);
    void           initImage(SVkFormatProps props, const Aquamarine::SDMABUFAttrs& attrs);
    void           initFB(VkRenderPass renderPass, int w, int h);

    VkFramebuffer  m_framebuffer = VK_NULL_HANDLE;
    SP<CVKTexture> m_tex;
};

class CVKRenderBuffer : public IRenderbuffer {
  public:
    CVKRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format);
    ~CVKRenderBuffer();

    virtual void bind() override;
    virtual void unbind() override;
};

class CVKFramebuffer : public IFramebuffer {
  public:
    CVKFramebuffer();
    ~CVKFramebuffer();

    bool                   readPixels(CHLBufferReference buffer, uint32_t offsetX = 0, uint32_t offsetY = 0) override;
    void                   release() override;
    void                   addStencil(SP<ITexture> tex) override;
    SP<CHyprVkFramebuffer> fb();

  protected:
    bool                   internalAlloc(int w, int h, uint32_t format = DRM_FORMAT_ARGB8888) override;
    SP<CHyprVkFramebuffer> m_FB;
    friend class CVKRenderBuffer;
};