#pragma once

#include "../Texture.hpp"
#include "render/vulkan/CommandBuffer.hpp"
#include "render/vulkan/DescriptorPool.hpp"
#include "render/vulkan/DeviceUser.hpp"
#include "render/vulkan/Framebuffer.hpp"
#include "render/vulkan/PipelineLayout.hpp"
#include "render/vulkan/types.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <vulkan/vulkan_core.h>

class CVKTexture;
class CHyprVkFramebuffer;

class CVKTextureView : public IDeviceUser {
  public:
    CVKTextureView(WP<CHyprVulkanDevice> device, CVKTexture* texture, WP<CVkPipelineLayout> layout);
    ~CVKTextureView();

    VkDescriptorSet vkDS();

  private:
    VkDescriptorSet       m_descriptorSet = VK_NULL_HANDLE;
    VkImageView           m_imageView     = VK_NULL_HANDLE;
    WP<CVKDescriptorPool> m_dsPool;

    friend class CVKTexture;
};

class CVKTexture : public ITexture {
  public:
    CVKTexture(CVKTexture&)        = delete;
    CVKTexture(CVKTexture&&)       = delete;
    CVKTexture(const CVKTexture&&) = delete;
    CVKTexture(const CVKTexture&)  = delete;

    CVKTexture(bool opaque = false);
    CVKTexture(uint32_t drmFormat, const Vector2D& size, bool keepDataCopy = false, bool opaque = false, VkImageUsageFlags flags = VULKAN_SHM_TEX_USAGE);
    CVKTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false);
    CVKTexture(const Aquamarine::SDMABUFAttrs&, bool opaque = false, VkImageUsageFlags flags = VULKAN_DMA_TEX_USAGE);
    ~CVKTexture();

    void setTexParameter(GLenum pname, GLint param) override;
    void allocate() override;
    void update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) override;
    // virtual void                bind() {};
    // virtual void                unbind() {};
    bool                     ok() override;
    bool                     isDMA() override;
    WP<CHyprVkCommandBuffer> m_lastUsedCB;

    VkImageView              vkView();
    SP<CVKTextureView>       getView(SP<CVkPipelineLayout> layout);
    bool                     read(uint32_t drmFformat, uint32_t stride, uint32_t width, uint32_t height, uint32_t srcX, uint32_t srcY, uint32_t dstX, uint32_t dstY, void* data);

    VkImage                  m_image = VK_NULL_HANDLE; // TODO private
  private:
    VkImageView                   createImageView();
    bool                          write(uint32_t stride, const CRegion& region, const void* data, VkImageLayout oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VkAccessFlags srcAccess = VK_ACCESS_SHADER_READ_BIT);

    VkImageView                   m_imageView = VK_NULL_HANDLE;
    std::array<VkDeviceMemory, 4> m_memory    = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    std::map<SP<CVkPipelineLayout>, SP<CVKTextureView>> m_views;
    bool                                                m_ok    = true;
    bool                                                m_isDMA = false;

    friend class CVKTextureView;
    friend class CHyprVkFramebuffer;
};
