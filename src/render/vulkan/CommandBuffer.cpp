#include "CommandBuffer.hpp"
#include "../../macros.hpp"
#include "render/vulkan/DeviceUser.hpp"
#include "utils.hpp"
#include <vulkan/vulkan_core.h>

CHyprVkCommandBuffer::CHyprVkCommandBuffer(WP<CHyprVulkanDevice> device) : IDeviceUser(device) {
    const VkCommandPoolCreateInfo cmdPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        // .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_device->queueFamilyIndex(),
    };

    if (vkCreateCommandPool(vkDevice(), &cmdPoolCreateInfo, nullptr, &m_cmdPool) != VK_SUCCESS) {
        CRIT("vkCreateCommandPool failed");
    }

    const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = m_cmdPool,
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
        vkDestroySemaphore(vkDevice(), m_waitSemaphore, nullptr);

    if (m_cmdBuffer)
        vkFreeCommandBuffers(vkDevice(), m_cmdPool, 1, &m_cmdBuffer);

    if (m_cmdPool)
        vkDestroyCommandPool(vkDevice(), m_cmdPool, nullptr);
}

void CHyprVkCommandBuffer::begin() {
    ASSERT(!m_recording);
    m_recording = true;

    if (vkResetCommandPool(vkDevice(), m_cmdPool, 0) != VK_SUCCESS) {
        CRIT("vkResetCommandPool failed");
    }

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(m_cmdBuffer, &beginInfo) != VK_SUCCESS) {
        CRIT("vkBeginCommandBuffer failed");
    }
}

void CHyprVkCommandBuffer::end(uint64_t signalPoint) {
    ASSERT(m_recording);

    // Ends recording of commands for the frame
    if (vkEndCommandBuffer(m_cmdBuffer) != VK_SUCCESS) {
        CRIT("vkEndCommandBuffer failed");
    }

    std::vector<VkSemaphoreSubmitInfo> waitSemaphores;
    std::vector<VkSemaphoreSubmitInfo> signalSemaphores;

    waitSemaphores.push_back({
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m_waitSemaphore,
        .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

    signalSemaphores.push_back({
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m_signalSemaphore,
        .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

    signalSemaphores.push_back({
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m_device->timelineSemaphore(),
        .value     = signalPoint,
        .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

    const std::array<VkCommandBufferSubmitInfo, 1> cmdBufferInfo{{{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
        .commandBuffer = m_cmdBuffer,
    }}};

    const std::array<VkSubmitInfo2, 1>             submitInfo{{{
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
        // .waitSemaphoreInfoCount   = waitSemaphores.size(),
        // .pWaitSemaphoreInfos      = waitSemaphores.data(),
                    .commandBufferInfoCount   = cmdBufferInfo.size(),
                    .pCommandBufferInfos      = cmdBufferInfo.data(),
                    .signalSemaphoreInfoCount = signalSemaphores.size(),
                    .pSignalSemaphoreInfos    = signalSemaphores.data(),
    }}};

    VkSemaphoreWaitInfoKHR                         waitInfo = {
                                .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
                                .semaphoreCount = 1,
                                .pSemaphores    = &m_signalSemaphore,
                                .pValues        = &signalPoint,
    };

    VkSemaphoreSignalInfo signalInfo = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = m_signalSemaphore,
        .value     = signalPoint,
    };

    VkSemaphoreSignalInfo timelineInfo = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = m_device->timelineSemaphore(),
        .value     = signalPoint,
    };

    VkSemaphoreSignalInfo waitSemInfo = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = m_waitSemaphore,
        .value     = signalPoint,
    };

    vkSignalSemaphore(vkDevice(), &signalInfo);
    // vkSignalSemaphore(vkDevice(), &timelineInfo);
    // vkSignalSemaphore(vkDevice(), &waitSemInfo);
    if (m_device->m_proc.vkQueueSubmit2KHR(m_device->queue(), uint32_t(submitInfo.size()), submitInfo.data(), VK_NULL_HANDLE) != VK_SUCCESS) {
        CRIT("vkQueueSubmit2 failed");
    }

    // if (m_device->m_proc.vkWaitSemaphoresKHR(vkDevice(), &waitInfo, 5000000000) != VK_SUCCESS) {
    //     CRIT("vkWaitSemaphoresKHR");
    // }

    m_recording = false;
};
