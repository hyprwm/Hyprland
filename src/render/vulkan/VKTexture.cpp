#include "VKTexture.hpp"
#include "Vulkan.hpp"
#include "debug/log/Logger.hpp"
#include "helpers/Format.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
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

CVKTextureView::CVKTextureView(WP<CHyprVulkanDevice> device, CVKTexture* texture, WP<CVkPipelineLayout> layout) : IDeviceUser(device), m_imageView(texture->createImageView()) {
    ;

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
        vkDestroyImageView(g_pHyprVulkan->vkDevice(), m_imageView, nullptr);
}

CVKTexture::CVKTexture(bool opaque) {
    m_opaque = opaque;
};

CVKTexture::CVKTexture(uint32_t drmFormat, const Vector2D& size, bool keepDataCopy, bool opaque, VkImageUsageFlags flags) :
    ITexture(drmFormat, nullptr, 0, size, keepDataCopy, opaque) {
    const auto device = g_pHyprVulkan->vkDevice();
    const auto props  = g_pHyprVulkan->device()->getFormat(drmFormat);

    if (!props.has_value() || props->format.isYCC) {
        Log::logger->log(Log::ERR, "Unsupported DRM format {}", NFormatUtils::drmFormatName(drmFormat));
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
        .usage         = flags,
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

    IF_VKFAIL(vkCreateImage, device, &imgInfo, nullptr, &m_image) {
        LOG_VKFAIL;
        return;
    }

    SET_VK_IMG_NAME(m_image, "empty ITexture");

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

    IF_VKFAIL(vkAllocateMemory, device, &memInfo, nullptr, &m_memory[0]) {
        LOG_VKFAIL;
        return;
    }

    IF_VKFAIL(vkBindImageMemory, device, m_image, m_memory[0], 0) {
        LOG_VKFAIL;
        return;
    }

    m_ok = true;
}

CVKTexture::CVKTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) :
    CVKTexture(drmFormat, size, keepDataCopy, opaque) {
    m_ok = write(stride, {0, 0, size.x, size.y}, pixels, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);
    if (m_ok)
        SET_VK_IMG_NAME(m_image, "SHM ITexture");
};

CVKTexture::CVKTexture(const Aquamarine::SDMABUFAttrs& attrs, bool opaque, VkImageUsageFlags flags) : ITexture(attrs.format, nullptr, 0, attrs.size, false, opaque), m_isDMA(true) {
    ;
    const auto    props      = g_pHyprVulkan->device()->getFormat(attrs.format).value();
    const uint8_t planeCount = attrs.planes;
    const auto    mod        = std::ranges::find_if(props.dmabuf.renderModifiers, [attrs](const auto m) { return m.props.drmFormatModifier == attrs.modifier; });

    if (attrs.size.x > mod->maxExtent.width || attrs.size.y > mod->maxExtent.height) {
        CRIT("DMA-BUF {}({}) is too large to import {}x{} > {}x{}", NFormatUtils::drmFormatName(attrs.format), NFormatUtils::drmModifierName(mod->props.drmFormatModifier),
             attrs.size.x, attrs.size.y, mod->maxExtent.width, mod->maxExtent.height);
    }

    if (mod->props.drmFormatModifierPlaneCount != planeCount) {
        CRIT("Number of planes {} != {}", planeCount, mod->props.drmFormatModifierPlaneCount);
    }

    // TODO support VK_FORMAT_FEATURE_DISJOINT_BIT

    VkExternalMemoryHandleTypeFlagBits htype = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkSubresourceLayout                planeLayouts[4] = {};

    for (int i = 0; i < planeCount; ++i) {
        planeLayouts[i].offset   = attrs.offsets[i];
        planeLayouts[i].rowPitch = attrs.strides[i];
        planeLayouts[i].size     = 0;
    }

    VkImageDrmFormatModifierExplicitCreateInfoEXT modInfo = {
        .sType                       = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
        .drmFormatModifier           = mod->props.drmFormatModifier,
        .drmFormatModifierPlaneCount = planeCount,
        .pPlaneLayouts               = planeLayouts,
    };

    VkExternalMemoryImageCreateInfo eimg = {
        .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .pNext       = &modInfo,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };

    VkImageCreateInfo imgInfo = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext       = &eimg,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = props.format.vkFormat,
        .extent      = {attrs.size.x, attrs.size.y, 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
        // .usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .usage         = flags,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (mod->canSrgb) {
        imgInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        VkFormat viewFormats[] = {
            props.format.vkFormat,
            props.format.vkSrgbFormat,
        };
        VkImageFormatListCreateInfoKHR listInfo = {
            .sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
            .viewFormatCount = sizeof(viewFormats) / sizeof(viewFormats[0]),
            .pViewFormats    = viewFormats,
        };
        modInfo.pNext = &listInfo;
    }

    if (vkCreateImage(g_pHyprVulkan->vkDevice(), &imgInfo, nullptr, &m_image) != VK_SUCCESS) {
        CRIT("vkCreateImage");
    }

    SET_VK_IMG_NAME(m_image, "DMA ITexture");

    VkBindImageMemoryInfo bindi[4] = {};

    const int             needMems = isDisjoint(attrs) ? planeCount : 1;
    for (int i = 0; i < needMems; ++i) {
        VkMemoryFdPropertiesKHR fdp = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
        };
        if (g_pHyprVulkan->device()->m_proc.vkGetMemoryFdPropertiesKHR(g_pHyprVulkan->vkDevice(), htype, attrs.fds[i], &fdp) != VK_SUCCESS) {
            CRIT("getMemoryFdPropertiesKHR");
        }

        VkImageMemoryRequirementsInfo2 memReqInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
            .image = m_image,
        };

        VkMemoryRequirements2 memReq = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        };

        vkGetImageMemoryRequirements2(g_pHyprVulkan->vkDevice(), &memReqInfo, &memReq);
        const auto memTypeIndex = findVkMemType(g_pHyprVulkan->device()->physicalDevice(), 0, memReq.memoryRequirements.memoryTypeBits & fdp.memoryTypeBits);
        if (memTypeIndex < 0) {
            CRIT("no valid memory type index");
        }

        int dfd = fcntl(attrs.fds[i], F_DUPFD_CLOEXEC, 0);
        if (dfd < 0) {
            CRIT("fcntl(F_DUPFD_CLOEXEC) failed");
        }

        VkMemoryDedicatedAllocateInfo dedicatedInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            .image = m_image,
        };

        VkImportMemoryFdInfoKHR importInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
            .pNext      = &dedicatedInfo,
            .handleType = htype,
            .fd         = dfd,
        };

        VkMemoryAllocateInfo memInfo = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = &importInfo,
            .allocationSize  = memReq.memoryRequirements.size,
            .memoryTypeIndex = memTypeIndex,
        };

        if (vkAllocateMemory(g_pHyprVulkan->vkDevice(), &memInfo, nullptr, &m_memory[i]) != VK_SUCCESS) {
            close(dfd);
            CRIT("vkAllocateMemory failed");
        }

        // fill bind info
        bindi[i].image        = m_image;
        bindi[i].memory       = m_memory[i];
        bindi[i].memoryOffset = 0;
        bindi[i].sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
    }

    if (vkBindImageMemory2(g_pHyprVulkan->vkDevice(), needMems, bindi) != VK_SUCCESS) {
        CRIT("vkBindMemory failed");
    }

    m_imageView = createImageView();
    if (!m_imageView)
        m_ok = false;
};

CVKTexture::~CVKTexture() {
    m_views.clear();

    if (m_imageView)
        vkDestroyImageView(g_pHyprVulkan->vkDevice(), m_imageView, nullptr);

    if (m_image) {
        vkDestroyImage(g_pHyprVulkan->vkDevice(), m_image, nullptr);
    }

    for (const auto mem : m_memory) {
        if (mem)
            vkFreeMemory(g_pHyprVulkan->vkDevice(), mem, nullptr);
    }
};

void CVKTexture::setTexParameter(GLenum pname, GLint param) {};
void CVKTexture::allocate() {};

void CVKTexture::update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) {
    write(stride, damage, pixels);
};

bool CVKTexture::ok() {
    return m_ok && m_memory[0] && m_image;
}

bool CVKTexture::isDMA() {
    return m_isDMA;
}

VkImageView CVKTexture::vkView() {
    if (!m_imageView)
        m_imageView = createImageView();
    return m_imageView;
}

SP<CVKTextureView> CVKTexture::getView(SP<CVkPipelineLayout> layout) {
    if (m_views.contains(layout))
        return m_views.at(layout);

    auto view = makeShared<CVKTextureView>(g_pHyprVulkan->device(), this, layout);
    m_views.insert({layout, view});
    return view;
}

bool CVKTexture::read(uint32_t drmFformat, uint32_t stride, uint32_t width, uint32_t height, uint32_t srcX, uint32_t srcY, uint32_t dstX, uint32_t dstY, void* data) {
    const auto         srcProps = g_pHyprVulkan->device()->getFormat(m_drmFormat).value();
    const auto         dstProps = g_pHyprVulkan->device()->getFormat(drmFformat).value();

    VkFormat           srcFormat      = srcProps.format.vkFormat;
    VkFormat           dstFormat      = dstProps.format.vkFormat;
    VkFormatProperties dstFormatProps = {0};
    VkFormatProperties srcFormatProps = {0};
    vkGetPhysicalDeviceFormatProperties(g_pHyprVulkan->device()->physicalDevice(), dstFormat, &dstFormatProps);
    vkGetPhysicalDeviceFormatProperties(g_pHyprVulkan->device()->physicalDevice(), srcFormat, &srcFormatProps);

    const bool blitSupported = srcFormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT && dstFormatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT;
    if (!blitSupported && srcFormat != dstFormat) {
        Log::logger->log(Log::ERR, "CVKTexture::read blit unsupported");
        return false;
    }

    VkImage        dstImage;
    VkDeviceMemory dstImgMemory;
    // bool use_cached = vk_renderer->read_pixels_cache.initialized && vk_renderer->read_pixels_cache.drm_format == drm_format && vk_renderer->read_pixels_cache.width == width &&
    //     vk_renderer->read_pixels_cache.height == height;

    // if (use_cached) {
    //     dst_image      = vk_renderer->read_pixels_cache.dst_image;
    //     dst_img_memory = vk_renderer->read_pixels_cache.dst_img_memory;
    // } else {
    VkImageCreateInfo imageCreateInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = dstFormat,
        .extent        = {.width = width, .height = height, .depth = 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_LINEAR,
        .usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    IF_VKFAIL(vkCreateImage, g_pHyprVulkan->vkDevice(), &imageCreateInfo, nullptr, &dstImage) {
        LOG_VKFAIL;
        return false;
    }

    SET_VK_IMG_NAME(dstImage, "read dstImage")

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(g_pHyprVulkan->vkDevice(), dstImage, &memReqs);

    const auto memTypeIndex =
        findVkMemType(g_pHyprVulkan->device()->physicalDevice(), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, memReqs.memoryTypeBits);

    if (memTypeIndex < 0) {
        Log::logger->log(Log::ERR, "findVkMemType failed");
        vkDestroyImage(g_pHyprVulkan->vkDevice(), dstImage, nullptr);
        return false;
    }

    VkMemoryAllocateInfo memAllocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memTypeIndex,
    };

    IF_VKFAIL(vkAllocateMemory, g_pHyprVulkan->vkDevice(), &memAllocInfo, nullptr, &dstImgMemory) {
        LOG_VKFAIL;
        vkDestroyImage(g_pHyprVulkan->vkDevice(), dstImage, nullptr);
        return false;
    }
    IF_VKFAIL(vkBindImageMemory, g_pHyprVulkan->vkDevice(), dstImage, dstImgMemory, 0) {
        LOG_VKFAIL;
        vkFreeMemory(g_pHyprVulkan->vkDevice(), dstImgMemory, nullptr);
        vkDestroyImage(g_pHyprVulkan->vkDevice(), dstImage, nullptr);
        return false;
    }

    // if (vk_renderer->read_pixels_cache.initialized) {
    //     vkFreeMemory(dev, vk_renderer->read_pixels_cache.dst_img_memory, NULL);
    //     vkDestroyImage(dev, vk_renderer->read_pixels_cache.dst_image, NULL);
    // }
    // vk_renderer->read_pixels_cache.initialized    = true;
    // vk_renderer->read_pixels_cache.drm_format     = drm_format;
    // vk_renderer->read_pixels_cache.dst_image      = dst_image;
    // vk_renderer->read_pixels_cache.dst_img_memory = dst_img_memory;
    // vk_renderer->read_pixels_cache.width          = width;
    // vk_renderer->read_pixels_cache.height         = height;
    // }

    const auto cb = g_pHyprVulkan->stageCB();
    if (!cb || cb->vk() == VK_NULL_HANDLE)
        return false;

    cb->changeLayout(dstImage, //
                     {.layout = VK_IMAGE_LAYOUT_UNDEFINED, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0},
                     {.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT});

    cb->changeLayout(m_image, //
                     {.layout = m_imageLayoutTemp, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_MEMORY_READ_BIT},
                     {.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_READ_BIT});
    m_imageLayoutTemp = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    if (blitSupported) {
        VkImageBlit imageBlitRegion = {.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
                                       .srcOffsets     = {{
                                                              .x = srcX,
                                                              .y = srcY,
                                                              .z = 0,
                                                      },
                                                          {
                                                              .x = srcX + width,
                                                              .y = srcY + height,
                                                              .z = 1,
                                                      }},
                                       .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
                                       .dstOffsets     = {{
                                                              .x = dstX,
                                                              .y = dstY,
                                                              .z = 0,
                                                      },
                                                          {
                                                              .x = dstX + width,
                                                              .y = dstY + height,
                                                              .z = 1,
                                                      }}};
        vkCmdBlitImage(cb->vk(), m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlitRegion, VK_FILTER_NEAREST);
    } else {
        VkImageCopy imageRegion = {.srcSubresource =
                                       {
                                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .layerCount = 1,
                                       },
                                   .srcOffset =
                                       {
                                           .x = srcX,
                                           .y = srcY,
                                       },
                                   .dstSubresource =
                                       {
                                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .layerCount = 1,
                                       },
                                   .extent = {
                                       .width  = width,
                                       .height = height,
                                       .depth  = 1,
                                   }};
        vkCmdCopyImage(cb->vk(), m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);
    }

    cb->changeLayout(dstImage, //
                     {.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                     {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    cb->changeLayout(m_image, //
                     {.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_READ_BIT},
                     {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_MEMORY_READ_BIT});
    m_imageLayoutTemp = VK_IMAGE_LAYOUT_GENERAL;

    if (!g_pHyprVulkan->submitStage())
        return false;

    VkImageSubresource imgSubRes = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel   = 0,
        .arrayLayer = 0,
    };
    VkSubresourceLayout imgSubLayout;
    vkGetImageSubresourceLayout(g_pHyprVulkan->vkDevice(), dstImage, &imgSubRes, &imgSubLayout);

    void* v;
    IF_VKFAIL(vkMapMemory, g_pHyprVulkan->vkDevice(), dstImgMemory, 0, VK_WHOLE_SIZE, 0, &v) {
        LOG_VKFAIL;
        return false;
    }

    VkMappedMemoryRange memRange = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = dstImgMemory,
        .offset = 0,
        .size   = VK_WHOLE_SIZE,
    };
    IF_VKFAIL(vkInvalidateMappedMemoryRanges, g_pHyprVulkan->vkDevice(), 1, &memRange) {
        LOG_VKFAIL;
        vkUnmapMemory(g_pHyprVulkan->vkDevice(), dstImgMemory);
        return false;
    }

    const char*    d          = (const char*)v + imgSubLayout.offset;
    unsigned char* p          = (unsigned char*)data + sc<size_t>(dstY * stride);
    uint32_t       bpp        = dstProps.bytesPerBlock();
    uint32_t       packStride = imgSubLayout.rowPitch;
    if (packStride == stride && dstX == 0) {
        memcpy(p, d, sc<size_t>(height) * stride);
    } else {
        for (size_t i = 0; i < height; ++i) {
            memcpy(p + (i * stride) + sc<size_t>(dstX * bpp), d + (i * packStride), sc<size_t>(width) * bpp);
        }
    }

    vkUnmapMemory(g_pHyprVulkan->vkDevice(), dstImgMemory);
    return true;
}

VkImageView CVKTexture::createImageView() {
    const auto            props = g_pHyprVulkan->device()->getFormat(m_drmFormat).value();

    VkImageView           imageView;

    VkImageViewCreateInfo viewInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image      = m_image,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = props.format.vkFormat,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = !m_opaque || props.format.isYCC ? VK_COMPONENT_SWIZZLE_IDENTITY : VK_COMPONENT_SWIZZLE_ONE},
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

    IF_VKFAIL(vkCreateImageView, g_pHyprVulkan->vkDevice(), &viewInfo, nullptr, &imageView) {
        LOG_VKFAIL;
        return VK_NULL_HANDLE;
    }

    return imageView;
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