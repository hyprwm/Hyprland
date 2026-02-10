#include "VKRenderer.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/Pipeline.hpp"
#include "./vulkan/RenderPass.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/PipelineLayout.hpp"
#include "./vulkan/Shaders.hpp"
#include "debug/log/Logger.hpp"
#include <hyprutils/memory/SharedPtr.hpp>
#include <vector>

CHyprVKRenderer::CHyprVKRenderer() : IHyprRenderer() {
    if (!m_shaders)
        m_shaders = makeShared<CVkShaders>(g_pHyprVulkan->m_device);

    if (!m_pipelineLayout)
        m_pipelineLayout = makeShared<CVkPipelineLayout>(g_pHyprVulkan->m_device);
}

void CHyprVKRenderer::initRender() {}

bool CHyprVKRenderer::initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    auto foundRP = std::ranges::find_if(m_renderPassList, [&](const auto& other) { return other->m_drmFormat == fmt; });
    if (foundRP != m_renderPassList.end())
        m_currentRenderPass = *foundRP;
    else
        m_currentRenderPass = m_renderPassList.emplace_back(makeShared<CVkRenderPass>(g_pHyprVulkan->m_device, fmt));

    if (!m_texturePipeline)
        m_texturePipeline = makeShared<CVkPipeline>(g_pHyprVulkan->m_device, m_currentRenderPass, m_pipelineLayout, m_shaders);

    auto foundFB = std::ranges::find_if(m_renderBuffers, [&](const auto& other) { return other->m_hlBuffer == buffer; });
    if (foundFB != m_renderBuffers.end())
        m_currentRenderbuffer = *foundFB;
    else
        m_currentRenderbuffer = m_renderBuffers.emplace_back(makeShared<CHyprVkFramebuffer>(g_pHyprVulkan->m_device, buffer, m_currentRenderPass->m_vkRenderPass));

    return true;
};

bool CHyprVKRenderer::beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple) {
    const auto            cb    = g_pHyprVulkan->begin();
    const auto            attrs = m_currentBuffer->dmabuf();

    VkRenderPassBeginInfo info = {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = m_currentRenderPass->m_vkRenderPass,
        .framebuffer = m_currentRenderbuffer->m_framebuffer,
        .renderArea =
            {
                .extent = {attrs.size.x, attrs.size.y},
            },
        .clearValueCount = 0,
    };
    vkCmdBeginRenderPass(cb->m_cmdBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {
        .width    = attrs.size.x,
        .height   = attrs.size.y,
        .maxDepth = 1,
    };
    vkCmdSetViewport(cb->m_cmdBuffer, 0, 1, &viewport);
    return true;
}

bool CHyprVKRenderer::beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, CFramebuffer* fb, bool simple) {
    return false;
};

static void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier2 imageBarrier = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext         = nullptr,
        .srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout     = currentLayout,
        .newLayout     = newLayout,
        .image         = image,
        .subresourceRange =
            {
                .aspectMask     = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount     = VK_REMAINING_ARRAY_LAYERS,
            },
    };

    VkDependencyInfo depInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                   = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &imageBarrier,
    };

    // vkCmdPipelineBarrier2(cmd, &depInfo);
}

void CHyprVKRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    static int frameNumber = 0;
    frameNumber++;

    for (const auto& el : m_renderPass.m_passElements) {
        Log::logger->log(Log::DEBUG, "m_passElements {}", el->element->passName());
    }

    vkCmdEndRenderPass(g_pHyprVulkan->m_commandBuffer->m_cmdBuffer);
    transitionImage(g_pHyprVulkan->m_commandBuffer->m_cmdBuffer, m_currentRenderbuffer->m_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearColorValue clearValue;
    float             flash = std::abs(std::sin(frameNumber / 120.f));
    clearValue              = {{0.0f, 0.0f, flash, 1.0f}};

    VkImageSubresourceRange clearRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };

    //clear image
    vkCmdClearColorImage(g_pHyprVulkan->m_commandBuffer->m_cmdBuffer, m_currentRenderbuffer->m_image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    //make the swapchain image into presentable mode
    transitionImage(g_pHyprVulkan->m_commandBuffer->m_cmdBuffer, m_currentRenderbuffer->m_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // size_t pass_textures_len = pass->textures.size / sizeof(struct wlr_vk_render_pass_texture);
    // size_t render_wait_cap   = pass_textures_len * WLR_DMABUF_MAX_PLANES;
    // render_wait              = calloc(render_wait_cap, sizeof(*render_wait));
    // if (render_wait == NULL) {
    //     wlr_log_errno(WLR_ERROR, "Allocation failed");
    //     goto error;
    // }

    std::vector<VkImageMemoryBarrier> acquireBarriers;
    std::vector<VkImageMemoryBarrier> releaseBarriers;
    // uint32_t              barrier_count    = wl_list_length(&renderer->foreign_textures) + 1;
    // VkImageMemoryBarrier* acquire_barriers = calloc(barrier_count, sizeof(*acquire_barriers));
    // VkImageMemoryBarrier* release_barriers = calloc(barrier_count, sizeof(*release_barriers));
    // if (acquire_barriers == NULL || release_barriers == NULL) {
    //     wlr_log_errno(WLR_ERROR, "Allocation failed");
    //     free(acquire_barriers);
    //     free(release_barriers);
    //     goto error;
    // }

    // struct wlr_vk_texture *texture, *tmp_tex;
    // size_t                 idx = 0;
    // wl_list_for_each_safe(texture, tmp_tex, &renderer->foreign_textures, foreign_link) {
    //     if (!texture->transitioned) {
    //         texture->transitioned = true;
    //     }

    //     // acquire
    //     acquire_barriers[idx] = (VkImageMemoryBarrier){
    //         .sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    //         .srcQueueFamilyIndex         = VK_QUEUE_FAMILY_FOREIGN_EXT,
    //         .dstQueueFamilyIndex         = renderer->dev->queue_family,
    //         .image                       = texture->image,
    //         .oldLayout                   = VK_IMAGE_LAYOUT_GENERAL,
    //         .newLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //         .srcAccessMask               = 0, // ignored anyways
    //         .dstAccessMask               = VK_ACCESS_SHADER_READ_BIT,
    //         .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    //         .subresourceRange.layerCount = 1,
    //         .subresourceRange.levelCount = 1,
    //     };

    //     // release
    //     release_barriers[idx] = (VkImageMemoryBarrier){
    //         .sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    //         .srcQueueFamilyIndex         = renderer->dev->queue_family,
    //         .dstQueueFamilyIndex         = VK_QUEUE_FAMILY_FOREIGN_EXT,
    //         .image                       = texture->image,
    //         .oldLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //         .newLayout                   = VK_IMAGE_LAYOUT_GENERAL,
    //         .srcAccessMask               = VK_ACCESS_SHADER_READ_BIT,
    //         .dstAccessMask               = 0, // ignored anyways
    //         .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    //         .subresourceRange.layerCount = 1,
    //         .subresourceRange.levelCount = 1,
    //     };

    //     ++idx;

    //     wl_list_remove(&texture->foreign_link);
    //     texture->owned = false;
    // }

    // uint32_t                           render_wait_len = 0;
    // struct wlr_vk_render_pass_texture* pass_texture;
    // wl_array_for_each(pass_texture, &pass->textures) {
    //     int sync_file_fds[WLR_DMABUF_MAX_PLANES];
    //     for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
    //         sync_file_fds[i] = -1;
    //     }

    //     if (pass_texture->wait_timeline) {
    //         int sync_file_fd = wlr_drm_syncobj_timeline_export_sync_file(pass_texture->wait_timeline, pass_texture->wait_point);
    //         if (sync_file_fd < 0) {
    //             wlr_log(WLR_ERROR, "Failed to export wait timeline point as sync_file");
    //             continue;
    //         }

    //         sync_file_fds[0] = sync_file_fd;
    //     } else {
    //         struct wlr_vk_texture* texture = pass_texture->texture;
    //         if (!vulkan_sync_foreign_texture(texture, sync_file_fds)) {
    //             wlr_log(WLR_ERROR, "Failed to wait for foreign texture DMA-BUF fence");
    //             continue;
    //         }
    //     }

    //     for (size_t i = 0; i < WLR_DMABUF_MAX_PLANES; i++) {
    //         if (sync_file_fds[i] < 0) {
    //             continue;
    //         }

    //         VkSemaphore sem = render_pass_wait_sync_file(pass, render_wait_len, sync_file_fds[i]);
    //         if (sem == VK_NULL_HANDLE) {
    //             close(sync_file_fds[i]);
    //             continue;
    //         }

    //         render_wait[render_wait_len] = (VkSemaphoreSubmitInfoKHR){
    //             .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
    //             .semaphore = sem,
    //             .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
    //         };

    //         render_wait_len++;
    //     }
    // }

    // also add acquire/release barriers for the current render buffer
    VkImageLayout srcLayout = VK_IMAGE_LAYOUT_GENERAL;
    // if (!pass->render_buffer_out->transitioned) {
    //     src_layout                            = VK_IMAGE_LAYOUT_PREINITIALIZED;
    //     pass->render_buffer_out->transitioned = true;
    // }

    // acquire render buffer before rendering
    acquireBarriers.push_back({
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0, // ignored anyways
        .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout           = srcLayout,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .dstQueueFamilyIndex = g_pHyprVulkan->m_device->queueFamilyIndex(),
        .image               = m_currentRenderbuffer->m_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    });

    // release render buffer after rendering
    releaseBarriers.push_back({
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask       = 0, // ignored anyways
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = g_pHyprVulkan->m_device->queueFamilyIndex(),
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .image               = m_currentRenderbuffer->m_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    });

    // vkCmdPipelineBarrier(stage_cb->vk, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0,
    //                      nullptr, acquireBarriers.size(), acquireBarriers.data());

    vkCmdPipelineBarrier(g_pHyprVulkan->m_commandBuffer->m_cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                         releaseBarriers.size(), releaseBarriers.data());

    g_pHyprVulkan->end();

    if (m_renderMode == RENDER_MODE_NORMAL)
        renderData().pMonitor->m_output->state->setBuffer(m_currentBuffer);

    if (renderingDoneCallback)
        renderingDoneCallback();

    m_currentBuffer = nullptr;
}
