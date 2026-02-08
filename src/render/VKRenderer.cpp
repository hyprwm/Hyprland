#include "VKRenderer.hpp"
#include "render/vulkan/Framebuffer.hpp"
#include <hyprutils/memory/SharedPtr.hpp>

CHyprVKRenderer::CHyprVKRenderer() : IHyprRenderer() {}

void CHyprVKRenderer::initRender() {}

bool CHyprVKRenderer::initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    struct wlr_vk_render_format_setup* render_setup = find_or_create_render_setup(renderer, &fmt->format, false, srgb);
    if (!render_setup)
        return false;

    auto found = std::ranges::find_if(m_renderBuffers, [&](const auto& other) { return other->m_hlBuffer == buffer; });
    if (found != m_renderBuffers.end())
        m_currentRenderbuffer = *found;
    else {
        auto fb = makeShared<CHyprVkFramebuffer>(buffer, fmt);
        m_renderBuffers.emplace_back(fb);
        m_currentRenderbuffer = fb;
    }

    VkFramebufferCreateInfo fb_info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .flags           = 0,
        .renderPass      = m_currentRenderPass,
        .attachmentCount = 1,
        .pAttachments    = &m_currentRenderbuffer->m_imageView,
        .width           = buffer->dmabuf().size.x,
        .height          = buffer->dmabuf().size.y,
        .layers          = 1,
    };

    if (vkCreateFramebuffer(g_pHyprVulkan->m_device->vkDevice(), &fb_info, nullptr, &m_currentRenderbuffer->m_framebuffer) != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "vkCreateFramebuffer failed");
        return false;
    }

    buffer->linear.render_setup = render_setup;

    return true;
};

bool CHyprVKRenderer::beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IHLBuffer> buffer, CFramebuffer* fb, bool simple) {
    g_pHyprVulkan->begin();
    return true;
}

void CHyprVKRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    g_pHyprVulkan->end();

    if (m_renderMode == RENDER_MODE_NORMAL)
        renderData().pMonitor->m_output->state->setBuffer(m_currentBuffer);

    if (renderingDoneCallback)
        renderingDoneCallback();

    m_currentBuffer = nullptr;
}
