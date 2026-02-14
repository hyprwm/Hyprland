#include "Framebuffer.hpp"
#include "../../debug/log/Logger.hpp"
#include "utils.hpp"
#include "types.hpp"
#include "DeviceUser.hpp"
#include <cstdint>
#include <fcntl.h>

CHyprVkFramebuffer::CHyprVkFramebuffer(WP<CHyprVulkanDevice> device, SP<Aquamarine::IBuffer> buffer, VkRenderPass renderPass) : IDeviceUser(device), m_hlBuffer(buffer) {
    const auto format = m_device->getFormat(buffer->dmabuf().format).value();

    initImage(format, buffer->dmabuf());
    initImageView(format.format.vkFormat);

    VkFramebufferCreateInfo fbInfo = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .flags           = 0,
        .renderPass      = renderPass,
        .attachmentCount = 1,
        .pAttachments    = &m_imageView,
        .width           = buffer->dmabuf().size.x,
        .height          = buffer->dmabuf().size.y,
        .layers          = 1,
    };

    if (vkCreateFramebuffer(vkDevice(), &fbInfo, nullptr, &m_framebuffer) != VK_SUCCESS) {
        CRIT("vkCreateFramebuffer failed");
    }
}

void CHyprVkFramebuffer::initImage(SVkFormatProps props, Aquamarine::SDMABUFAttrs attrs) {
    const uint8_t planeCount = attrs.planes;
    const auto    mod        = std::ranges::find_if(props.dmabuf.renderModifiers, [attrs](const auto m) { return m.props.drmFormatModifier == attrs.modifier; });

    if (attrs.size.x > mod->maxExtent.width || attrs.size.y > mod->maxExtent.height) {
        CRIT("DMA-BUF is too large to import {}x{} > {}x{}", attrs.size.x, attrs.size.y, mod->maxExtent.width, mod->maxExtent.height);
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
        .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
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

    if (vkCreateImage(vkDevice(), &imgInfo, nullptr, &m_image) != VK_SUCCESS) {
        CRIT("vkCreateImage");
    }

    VkBindImageMemoryInfo bindi[4] = {};

    for (int i = 0; i < planeCount; ++i) {
        VkMemoryFdPropertiesKHR fdp = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
        };
        if (m_device->m_proc.vkGetMemoryFdPropertiesKHR(vkDevice(), htype, attrs.fds[i], &fdp) != VK_SUCCESS) {
            CRIT("getMemoryFdPropertiesKHR");
        }

        VkImageMemoryRequirementsInfo2 memReqInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
            .image = m_image,
        };

        VkMemoryRequirements2 memReq = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        };

        vkGetImageMemoryRequirements2(vkDevice(), &memReqInfo, &memReq);
        const auto memTypeIndex = findVkMemType(m_device->physicalDevice(), 0, memReq.memoryRequirements.memoryTypeBits & fdp.memoryTypeBits);
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

        if (vkAllocateMemory(vkDevice(), &memInfo, nullptr, &m_memory[i]) != VK_SUCCESS) {
            close(dfd);
            CRIT("vkAllocateMemory failed");
        }

        // fill bind info
        bindi[i].image        = m_image;
        bindi[i].memory       = m_memory[i];
        bindi[i].memoryOffset = 0;
        bindi[i].sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
    }

    if (vkBindImageMemory2(vkDevice(), planeCount, bindi) != VK_SUCCESS) {
        CRIT("vkBindMemory failed");
    }
}

void CHyprVkFramebuffer::initImageView(VkFormat format) {
    VkImageViewCreateInfo viewInfo = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    };

    if (vkCreateImageView(vkDevice(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        CRIT("vkCreateImageView failed");
    }
}

CHyprVkFramebuffer::~CHyprVkFramebuffer() {
    if (m_framebuffer)
        vkDestroyFramebuffer(vkDevice(), m_framebuffer, nullptr);

    if (m_imageView)
        vkDestroyImageView(vkDevice(), m_imageView, nullptr);

    if (m_image)
        vkDestroyImage(vkDevice(), m_image, nullptr);

    for (const auto mem : m_memory) {
        if (mem)
            vkFreeMemory(vkDevice(), mem, nullptr);
    }
}