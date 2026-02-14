#include "VKTexture.hpp"
#include "Vulkan.hpp"
#include "debug/log/Logger.hpp"
#include "helpers/Format.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "render/vulkan/CommandBuffer.hpp"
#include "render/vulkan/DeviceUser.hpp"
#include "render/vulkan/utils.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "MemorySpan.hpp"

CVKTextureView::CVKTextureView(WP<CHyprVulkanDevice> device, CVKTexture* texture, WP<CVkPipelineLayout> layout) : IDeviceUser(device) {
    const auto            props = g_pHyprVulkan->device()->getFormat(texture->m_drmFormat).value();

    VkImageViewCreateInfo viewInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image      = texture->m_image,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = props.format.vkFormat,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = !texture->m_opaque || props.format.isYCC ? VK_COMPONENT_SWIZZLE_IDENTITY : VK_COMPONENT_SWIZZLE_ONE},
        .subresourceRange =
            {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    };

    // TODO support YCC

    if (vkCreateImageView(vkDevice(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "vkCreateImageView failed");
        return;
    }

    m_dsPool = g_pHyprVulkan->allocateDescriptorSet(layout->descriptorSet(), &m_descriptorSet);
    if (!m_dsPool) {
        Log::logger->log(Log::ERR, "failed to allocate descriptor");
        return;
    }

    VkDescriptorImageInfo dsInfo = {
        .imageView   = m_imageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet dsWrite = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = m_descriptorSet,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &dsInfo,
    };

    vkUpdateDescriptorSets(vkDevice(), 1, &dsWrite, 0, nullptr);
}

VkDescriptorSet CVKTextureView::vkDS() {
    return m_descriptorSet;
}

CVKTextureView::~CVKTextureView() {
    if (m_descriptorSet)
        vkFreeDescriptorSets(vkDevice(), m_dsPool->vkPool(), 1, &m_descriptorSet);

    if (m_imageView)
        vkDestroyImageView(vkDevice(), m_imageView, nullptr);
}

CVKTexture::CVKTexture(bool opaque) {
    m_opaque = opaque;
};

CVKTexture::CVKTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) :
    ITexture(drmFormat, pixels, stride, size, keepDataCopy, opaque) {
    const auto device = g_pHyprVulkan->vkDevice();
    const auto props  = g_pHyprVulkan->device()->getFormat(drmFormat);

    if (!props.has_value() || props->format.isYCC) {
        Log::logger->log(Log::ERR, "Unsupported DRM format", NFormatUtils::drmFormatName(drmFormat));
        return;
    }

    if (size.x > props->shm.maxExtent.width || size.y > props->shm.maxExtent.height) {
        Log::logger->log(Log::ERR, "Texture is too large {}x{} > {}x{}", size.x, size.y, props->shm.maxExtent.width, props->shm.maxExtent.height);
        return;
    }

    VkImageCreateInfo imgInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = props->format.vkFormat,
        .extent        = {size.x, size.y, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VULKAN_SHM_TEX_USAGE,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (props->shm.canSrgb) {
        imgInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        const auto                     viewFormats = std::to_array({
            props->format.vkFormat,
            props->format.vkSrgbFormat,
        });
        VkImageFormatListCreateInfoKHR listInfo    = {
               .sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
               .viewFormatCount = viewFormats.size(),
               .pViewFormats    = viewFormats.data(),
        };
        imgInfo.pNext = &listInfo;
    }

    if (vkCreateImage(device, &imgInfo, nullptr, &m_image) != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "vkCreateImage failed");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_image, &memReqs);

    const auto memTypeIndex = findVkMemType(g_pHyprVulkan->device()->physicalDevice(), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memReqs.memoryTypeBits);
    if (memTypeIndex == -1) {
        Log::logger->log(Log::ERR, "failed to find suitable vulkan memory type");
        return;
    }

    VkMemoryAllocateInfo memInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memTypeIndex,
    };

    if (vkAllocateMemory(device, &memInfo, nullptr, &m_memory) != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "vkAllocatorMemory failed");
        return;
    }

    if (vkBindImageMemory(device, m_image, m_memory, 0) != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "vkBindImageMemory failed");
        return;
    }

    m_ok = write(stride, {0, 0, size.x, size.y}, pixels, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);
};

CVKTexture::CVKTexture(const Aquamarine::SDMABUFAttrs&, void* image, bool opaque) {
    m_opaque = opaque;
};

CVKTexture::~CVKTexture() {
    Log::logger->log(Log::DEBUG, "CVKTexture::~CVKTexture {}x{}", m_size.x, m_size.y);
    m_views.clear();

    if (m_image)
        vkDestroyImage(g_pHyprVulkan->vkDevice(), m_image, nullptr);

    if (m_memory)
        vkFreeMemory(g_pHyprVulkan->vkDevice(), m_memory, nullptr);
};

void CVKTexture::setTexParameter(GLenum pname, GLint param) {};
void CVKTexture::allocate() {};

void CVKTexture::update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) {
    write(stride, damage, pixels);
};

bool CVKTexture::ok() {
    return m_ok && m_memory && m_image;
}

WP<CVKTextureView> CVKTexture::getView(SP<CVkPipelineLayout> layout) {
    if (m_views.contains(layout))
        return m_views.at(layout);

    auto view = makeShared<CVKTextureView>(g_pHyprVulkan->device(), this, layout);
    m_views.insert({layout, view});
    return view;
}

bool CVKTexture::write(uint32_t stride, const CRegion& region, const void* data, VkImageLayout oldLayout, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess) {
    const auto props = g_pHyprVulkan->device()->getFormat(m_drmFormat).value();

    uint32_t   bsize = 0;

    const auto rects = region.getRects();
    for (const auto& rect : rects) {
        uint32_t width  = rect.x2 - rect.x1;
        uint32_t height = rect.y2 - rect.y1;

        // make sure assumptions are met
        ASSERT(rect.x2 <= m_size.x);
        ASSERT(rect.y2 <= m_size.y);

        bsize += height * props.minStride(width);
    }

    std::vector<VkBufferImageCopy> copies(rects.size());
    const auto                     span = g_pHyprVulkan->getMemorySpan(bsize, props.bytesPerBlock());
    if (!span.valid()) {
        Log::logger->log(Log::ERR, "Failed to get memory span of size {}({})", bsize, props.bytesPerBlock());
        return false;
    }

    auto     map       = span->cpuMapping();
    uint32_t bufOffset = span->offset();
    int      i         = 0;
    for (const auto& rect : rects) {
        const uint32_t width        = rect.x2 - rect.x1;
        const uint32_t height       = rect.y2 - rect.y1;
        const uint32_t srcX         = rect.x1;
        const uint32_t srcY         = rect.y1;
        const uint32_t packedStride = props.minStride(width);

        const char*    pdata = (char*)data;
        pdata += (size_t)stride * srcY;
        pdata += (size_t)props.format.bytesPerBlock * srcX;
        if (srcX == 0 && width == m_size.x && stride == packedStride) {
            memcpy(map, pdata, (size_t)packedStride * height);
            map += (size_t)packedStride * height;
        } else {
            for (unsigned y = 0; y < height; ++y) {
                memcpy(map, pdata, packedStride);
                pdata += stride;
                map += packedStride;
            }
        }

        copies[i] = {
            .bufferOffset      = bufOffset,
            .bufferRowLength   = width,
            .bufferImageHeight = height,
            .imageSubresource =
                {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            .imageOffset = {srcX, srcY, 0},
            .imageExtent = {width, height, 1},
        };

        bufOffset += height * packedStride;
        i++;
    }

    const auto cb = g_pHyprVulkan->stageCB();
    if (!cb || cb->vk() == VK_NULL_HANDLE)
        return false;

    const CHyprVkCommandBuffer::SImageLayoutSettings tmpLayout = {
        .layout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .stageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };

    cb->changeLayout(m_image,
                     {
                         .layout     = oldLayout,
                         .stageMask  = srcStage,
                         .accessMask = srcAccess,
                     },
                     tmpLayout);

    vkCmdCopyBufferToImage(cb->vk(), span->vkBuffer(), m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copies.size(), copies.data());
    cb->changeLayout(m_image, tmpLayout,
                     {
                         .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         .stageMask  = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                         .accessMask = VK_ACCESS_SHADER_READ_BIT,
                     });

    m_lastUsedCB = cb;

    return true;
}