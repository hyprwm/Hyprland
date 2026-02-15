#include "CommandBuffer.hpp"
#include "../../macros.hpp"
#include "debug/log/Logger.hpp"
#include "render/vulkan/DeviceUser.hpp"
#include "render/vulkan/VKTexture.hpp"
#include "utils.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>

CHyprVkCommandBuffer::CHyprVkCommandBuffer(WP<CHyprVulkanDevice> device) : IDeviceUser(device) {
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = m_device->commandPool(),
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (vkAllocateCommandBuffers(vkDevice(), &commandBufferAllocateInfo, &m_cmdBuffer) != VK_SUCCESS) {
        CRIT("vkAllocateCommandBuffers failed");
    }

    const VkSemaphoreCreateInfo semaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(vkDevice(), &semaphoreCreateInfo, nullptr, &m_waitSemaphore) != VK_SUCCESS) {
        CRIT("vkCreateSemaphore failed");
    }

    const VkSemaphoreTypeCreateInfoKHR semaphoreTypeInfo = {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
        .initialValue  = 0,
    };
    const VkSemaphoreCreateInfo timelineCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &semaphoreTypeInfo};

    if (vkCreateSemaphore(vkDevice(), &timelineCreateInfo, nullptr, &m_signalSemaphore) != VK_SUCCESS) {
        CRIT("vkCreateSemaphore failed");
    }
}

CHyprVkCommandBuffer::~CHyprVkCommandBuffer() {
    if (!m_device)
        return;

    if (m_waitSemaphore)
        vkDestroySemaphore(vkDevice(), m_waitSemaphore, nullptr);

    if (m_signalSemaphore)
        vkDestroySemaphore(vkDevice(), m_signalSemaphore, nullptr);

    if (m_cmdBuffer)
        vkFreeCommandBuffers(vkDevice(), m_device->commandPool(), 1, &m_cmdBuffer);
}

void CHyprVkCommandBuffer::begin() {
    ASSERT(!m_recording);

    resetUsedResources();

    m_recording = true;

    // if (vkResetCommandPool(vkDevice(), m_device->commandPool(), 0) != VK_SUCCESS) {
    //     CRIT("vkResetCommandPool failed");
    // }

    if (vkResetCommandBuffer(m_cmdBuffer, 0) != VK_SUCCESS) {
        CRIT("vkResetCommandBuffer failed");
    }

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        // .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(m_cmdBuffer, &beginInfo) != VK_SUCCESS) {
        CRIT("vkBeginCommandBuffer failed");
    }
}

void CHyprVkCommandBuffer::end(uint64_t signalPoint) {
    ASSERT(m_recording);
    m_timelinePoint = signalPoint;

    // Ends recording of commands for the frame
    if (vkEndCommandBuffer(m_cmdBuffer) != VK_SUCCESS) {
        CRIT("vkEndCommandBuffer failed");
    }

    m_recording = false;
};

VkCommandBuffer CHyprVkCommandBuffer::vk() {
    return m_cmdBuffer;
}

void CHyprVkCommandBuffer::changeLayout(VkImage img, const SImageLayoutSettings& src, const SImageLayoutSettings& dst) {
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = src.accessMask,
        .dstAccessMask       = dst.accessMask,
        .oldLayout           = src.layout,
        .newLayout           = dst.layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = img,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    vkCmdPipelineBarrier(m_cmdBuffer, src.stageMask, dst.stageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool CHyprVkCommandBuffer::busy() {
    return m_recording;
}

void CHyprVkCommandBuffer::useTexture(SP<ITexture> tex) {
    m_usedTextures.push_back(tex);
}

void CHyprVkCommandBuffer::useFB(SP<CHyprVkFramebuffer> fb) {
    m_usedFB = fb;
}

void CHyprVkCommandBuffer::resetUsedResources() {
    // return;
    // if ((!m_usedFB || !m_usedFB->texture()) && !m_usedTextures.size())
    //     return;
    // Log::logger->log(Log::DEBUG, "resetUsedResources {:x}", rc<uintptr_t>(m_cmdBuffer));
    // for (const auto& t : m_usedTextures)
    // Log::logger->log(Log::DEBUG, "    used tex {:x} {}x{}", rc<uintptr_t>(dc<CVKTexture*>(t.get())->m_image), t->m_size.x, t->m_size.y);
    m_usedTextures.clear();
    // if (m_usedFB && m_usedFB->texture())
    // Log::logger->log(Log::DEBUG, "    used fb tex {:x}", rc<uintptr_t>(m_usedFB->texture()->m_image));
    m_usedFB.reset();
}