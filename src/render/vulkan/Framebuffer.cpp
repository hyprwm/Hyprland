#include "Framebuffer.hpp"
#include "../../debug/log/Logger.hpp"
#include "helpers/Format.hpp"
#include "render/Framebuffer.hpp"
#include "render/Renderbuffer.hpp"
#include "render/Renderer.hpp"
#include "render/VKRenderer.hpp"
#include "render/vulkan/VKTexture.hpp"
#include "render/vulkan/Vulkan.hpp"
#include "utils.hpp"
#include "types.hpp"
#include "DeviceUser.hpp"
#include <cstdint>
#include <fcntl.h>
#include <format>
#include <hyprgraphics/egl/Egl.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <vulkan/vulkan_core.h>

using namespace Render::VK;

CHyprVkFramebuffer::CHyprVkFramebuffer(WP<CHyprVulkanDevice> device, int w, int h, uint32_t fmt, SP<ITexture> stencilTex) : IDeviceUser(device), m_stencilTex(stencilTex) {
    const auto format = m_device->getFormat(fmt).value();

    initImage(format, w, h);
}

CHyprVkFramebuffer::CHyprVkFramebuffer(WP<CHyprVulkanDevice> device, SP<Aquamarine::IBuffer> buffer) : IDeviceUser(device) {
    const auto format = m_device->getFormat(buffer->dmabuf().format).value();

    initImage(format, buffer->dmabuf());
}

void CHyprVkFramebuffer::initImage(SVkFormatProps props, int w, int h) {
    const Vector2D size = {w, h};
    m_tex = makeShared<CVKTexture>(props.format.drmFormat, size, false, false,
                                   VULKAN_DMA_TEX_USAGE | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                       VK_IMAGE_USAGE_SAMPLED_BIT);
}

void CHyprVkFramebuffer::initImage(SVkFormatProps props, const Aquamarine::SDMABUFAttrs& attrs) {
    m_tex = makeShared<CVKTexture>(attrs, false,
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
}

VkFramebuffer CHyprVkFramebuffer::vk() {
    return m_framebuffer;
}

VkImage CHyprVkFramebuffer::vkImage() {
    return m_tex->m_image;
}

SP<CVKTexture> CHyprVkFramebuffer::texture() {
    return m_tex;
}

CHyprVkFramebuffer::~CHyprVkFramebuffer() {
    if (m_framebuffer)
        vkDestroyFramebuffer(vkDevice(), m_framebuffer, nullptr);
}

CVKRenderBuffer::CVKRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format) : IRenderbuffer(buffer, format) {
    m_framebuffer                                  = makeShared<CVKFramebuffer>();
    m_framebuffer->m_drmFormat                     = format;
    m_framebuffer->m_size                          = buffer->size;
    auto fb                                        = makeShared<CHyprVkFramebuffer>(g_pHyprVulkan->device(), buffer);
    dc<CVKFramebuffer*>(m_framebuffer.get())->m_FB = fb;
    dc<CVKFramebuffer*>(m_framebuffer.get())->setTexture(fb->texture());
    m_good = true;
}

CVKRenderBuffer::~CVKRenderBuffer() = default;

void CVKRenderBuffer::bind() {
    LOGM(Log::WARN, "fixme: unimplemented bind");
}

void CVKRenderBuffer::unbind() {
    LOGM(Log::WARN, "fixme: unimplemented unbind");
}

CVKFramebuffer::CVKFramebuffer() : IFramebuffer() {}
CVKFramebuffer::CVKFramebuffer(const std::string& name) : IFramebuffer(name) {}

CVKFramebuffer::~CVKFramebuffer() {
    m_FB.reset();
}

void CVKFramebuffer::bind() {
    LOGM(Log::TRACE, "CVKFramebuffer::bind() is a noop. should be called only from IHyprRenderer::bindFB");
}

bool CVKFramebuffer::readPixels(CHLBufferReference buffer, uint32_t offsetX, uint32_t offsetY, uint32_t width, uint32_t height) {
    if (!m_tex)
        return false;

    auto shm                      = buffer->shm();
    auto [pixelData, fmt, bufLen] = buffer->beginDataPtr(0); // no need for end, cuz it's shm

    const auto PFORMAT = Hyprgraphics::Egl::getPixelFormatFromDRM(shm.format);
    if (!PFORMAT) {
        LOGM(Log::ERR, "Can't copy: failed to find a pixel format");
        return false;
    }

    uint32_t packStride = Hyprgraphics::Egl::minStride(PFORMAT, m_size.x);

    LOGM(Log::DEBUG, "Read from tex {}x{}@{}x{}", m_size.x, m_size.y, offsetX, offsetY);
    VKTEX(m_tex)->m_lastKnownLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return VKTEX(m_tex)->read(shm.format, packStride, width > 0 ? width : m_size.x, height > 0 ? height : m_size.y, offsetX, offsetY, 0, 0, pixelData);
}

void CVKFramebuffer::release() {
    m_FB.reset();
    m_fbAllocated = false;
};

void CVKFramebuffer::addStencil(SP<ITexture> tex) {
    if (m_stencilTex == tex)
        return;

    RASSERT(!m_fbAllocated, "Should add stencil tex prior to FB allocation")
    m_stencilTex = tex;
};

void CVKFramebuffer::setName(const std::string& name) {
    m_name = name;
    if (m_FB && m_FB->texture() && m_FB->texture()->ok())
        SET_VK_IMG_NAME(m_FB->vkImage(), std::format("IFramebuffer '{}' {} {}x{}", m_name.length() ? m_name : "?", NFormatUtils::drmFormatName(m_drmFormat), m_size.x, m_size.y));
}

bool CVKFramebuffer::internalAlloc(int w, int h, uint32_t fmt) {
    m_drmFormat = fmt;
    m_size      = {w, h};
    m_FB        = makeShared<CHyprVkFramebuffer>(g_pHyprVulkan->device(), w, h, fmt, m_stencilTex);
    if (m_FB)
        m_tex = m_FB->texture();
    if (m_tex && m_tex->ok()) {
        setName(m_name);
        g_pHyprVulkan->stageCB()->changeLayout(m_FB->vkImage(), //
                                               {.layout = VK_IMAGE_LAYOUT_UNDEFINED, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                                               {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
        m_lastKnownLayout   = VK_IMAGE_LAYOUT_GENERAL;
        m_FB->m_initialized = true;
    }
    m_fbAllocated = true;
    return m_FB;
};

void CVKFramebuffer::setTexture(SP<ITexture> tex) {
    m_tex = tex;
}

SP<CHyprVkFramebuffer> CVKFramebuffer::fb() {
    return m_FB;
}
