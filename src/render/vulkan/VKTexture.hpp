#pragma once

#include "../Texture.hpp"
#include "render/vulkan/CommandBuffer.hpp"
#include "render/vulkan/DescriptorPool.hpp"
#include "render/vulkan/DeviceUser.hpp"
#include "render/vulkan/PipelineLayout.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <vulkan/vulkan_core.h>

class CVKTexture;

class CVKTextureView : public IDeviceUser {
  public:
    CVKTextureView(WP<CHyprVulkanDevice> device, CVKTexture* texture, WP<CVkPipelineLayout> layout);
    ~CVKTextureView();

    VkDescriptorSet vkDS();

  private:
    VkImageView           m_imageView;
    VkDescriptorSet       m_descriptorSet;
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
    CVKTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false);
    CVKTexture(const Aquamarine::SDMABUFAttrs&, void* image, bool opaque = false);
    ~CVKTexture();

    void setTexParameter(GLenum pname, GLint param) override;
    void allocate() override;
    void update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) override;
    // virtual void                bind() {};
    // virtual void                unbind() {};
    bool ok() override;
    // virtual bool                isDMA();
    WP<CHyprVkCommandBuffer> m_lastUsedCB;

    WP<CVKTextureView>       getView(SP<CVkPipelineLayout> layout);

  private:
    bool           write(uint32_t stride, const CRegion& region, const void* data, VkImageLayout oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VkAccessFlags srcAccess = VK_ACCESS_SHADER_READ_BIT);

    VkImage        m_image;
    VkDeviceMemory m_memory;
    std::map<SP<CVkPipelineLayout>, SP<CVKTextureView>> m_views;
    bool                                                m_ok = true;

    friend class CVKTextureView;
};
