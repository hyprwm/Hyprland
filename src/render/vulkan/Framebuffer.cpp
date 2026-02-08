#include "Framebuffer.hpp"
#include "Vulkan.hpp"
#include "../../debug/log/Logger.hpp"
#include "macros.hpp"

CHyprVkFramebuffer::CHyprVkFramebuffer(SP<Aquamarine::IBuffer> buffer, DRMFormat fmt) : m_hlBuffer(buffer), m_drmFormat(fmt) {
    const auto            format = g_pHyprVulkan->m_device->getFormat(fmt).value();

    VkImageViewCreateInfo view_info = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = format.format.vkFormat,
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

    if (vkCreateImageView(g_pHyprVulkan->m_device->vkDevice(), &view_info, nullptr, &m_imageView) != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "vkCreateImageView failed");
        ASSERT(false); // FIXME
    }
}

CHyprVkFramebuffer::~CHyprVkFramebuffer() {}