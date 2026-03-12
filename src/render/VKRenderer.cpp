#include "VKRenderer.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/Pipeline.hpp"
#include "./vulkan/RenderPass.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/PipelineLayout.hpp"
#include "./vulkan/Shaders.hpp"
#include "./vulkan/utils.hpp"
#include "../debug/log/Logger.hpp"
#include "../macros.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "./Framebuffer.hpp"
#include "./OpenGL.hpp"
#include "./Renderer.hpp"
#include "./ShaderLoader.hpp"
#include "./pass/TexPassElement.hpp"
#include "./vulkan/BlurPass.hpp"
#include "./vulkan/BorderGradientUBO.hpp"
#include "./vulkan/VKElementRenderer.hpp"
#include "./vulkan/VKTexture.hpp"
#include "./vulkan/types.hpp"
#include <algorithm>
#include <array>
#include <cairo.h>
#include <cstdint>
#include <cstring>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Mat3x3.hpp>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace Render;
using namespace Render::VK;

CHyprVKRenderer::CHyprVKRenderer() : IHyprRenderer() {
    if (!m_shaders)
        m_shaders = makeShared<CVkShaders>(g_pHyprVulkan->m_device);
}

void CHyprVKRenderer::initRender() {
    ASSERT(!m_busy);
    m_busy = true;
    m_currentPipeline.reset();
    // for (const auto& cb : g_pHyprVulkan->m_commandBuffers) {
    //     if (!cb->busy())
    //         cb->resetUsedResources();
    // }
}

SP<CVkRenderPass> CHyprVKRenderer::getRenderPass(uint32_t fmt) {
    auto foundRP = std::ranges::find_if(m_renderPassList, [&](const auto& other) { return other->m_drmFormat == fmt; });
    if (foundRP != m_renderPassList.end())
        return *foundRP;
    else
        return m_renderPassList.emplace_back(makeShared<CVkRenderPass>(g_pHyprVulkan->m_device, fmt, m_shaders));
}

SP<CVKBlurPass> CHyprVKRenderer::getBlurPass(uint32_t fmt) {
    static auto PBLURPASSES = CConfigValue<Hyprlang::INT>("decoration:blur:passes");
    const auto  BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));

    auto        foundRP = std::ranges::find_if(m_blurPassList, [&](const auto& other) { return other->format() == fmt; });
    if (foundRP != m_blurPassList.end()) {
        if ((*foundRP)->passes() != BLUR_PASSES)
            *foundRP = makeShared<CVKBlurPass>(g_pHyprVulkan->m_device, fmt, m_shaders, BLUR_PASSES);
        return *foundRP;
    } else
        return m_blurPassList.emplace_back(makeShared<CVKBlurPass>(g_pHyprVulkan->m_device, fmt, m_shaders, BLUR_PASSES));
}

bool CHyprVKRenderer::initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    m_currentDrmFormat = fmt;
    m_renderData.outFB = getOrCreateRenderbuffer(buffer, fmt)->getFB();

    return true;
};

bool CHyprVKRenderer::beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple) {
    initRender();

    m_renderData.outFB = fb;
    m_currentDrmFormat = m_renderData.outFB->m_drmFormat;

    return beginRenderInternal(pMonitor, damage, simple);
};

SP<ITexture> CHyprVKRenderer::blurFramebuffer(SP<IFramebuffer> source, float a, CRegion* originalDamage) {
    const auto bp = getBlurPass(m_currentRenderbuffer->texture()->m_drmFormat);
    m_currentCommandBuffer->useBlurPass(bp);
    if (m_hasBoundFB)
        m_currentCommandBuffer->endRenderPass();

    m_currentCommandBuffer->changeLayout(VKTEX(source->getTexture())->m_image, //
                                         {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                                         {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    static auto PBLURSIZE   = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURPASSES = CConfigValue<Hyprlang::INT>("decoration:blur:passes");
    const auto  BLUR_PASSES = std::clamp(*PBLURPASSES, sc<int64_t>(1), sc<int64_t>(8));

    // prep damage
    CRegion damage{*originalDamage};
    damage.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                     m_renderData.pMonitor->m_transformedSize.y);
    damage.expand(std::clamp(*PBLURSIZE, sc<int64_t>(1), sc<int64_t>(40)) * pow(2, BLUR_PASSES));
    const auto tex =
        bp->blurTexture(source->getTexture(), m_renderData.pMonitor->resources()->getUnusedWorkBuffer(), m_renderData.pMonitor->resources()->getUnusedWorkBuffer(), a, damage);

    m_currentCommandBuffer->changeLayout(
        VKTEX(source->getTexture())->m_image, //
        {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
        {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    if (m_hasBoundFB)
        startRenderPassHelper(m_currentRenderPass->m_vkRenderPass, m_hasBoundFB->vk(), m_hasBoundFB->texture()->m_size, m_currentCommandBuffer->vk());
    return tex;
}

void CHyprVKRenderer::renderOffToMain(SP<IFramebuffer> off) {
    CBox       newBox = {0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};

    const auto tex = off->getTexture();
    RASSERT(m_renderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex->ok()), "Attempted to draw nullptr texture!");

    TRACY_GPU_ZONE("RenderTexturePrimitive");

    if (m_renderData.damage.empty())
        return;

    m_renderData.renderModif.applyToBox(newBox);

    const auto cb = g_pHyprVulkan->renderCB();
    cb->useTexture(tex);

    const auto  mat = projectBoxToTarget(newBox).getMatrix();

    const auto& vertData = matToVertShader(mat);

    const auto  layout = m_currentRenderPass->passPipeline()->layout().lock();
    const auto  view   = VKTEX(tex)->getView(layout);
    if (!view)
        return;

    bindPipeline(m_currentRenderPass->passPipeline());

    const auto ds = view->vkDS();
    vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);

    // TODO
    // ensure the final blit uses the desired sampling filter
    // when cursor zoom is active we want nearest-neighbor (no anti-aliasing)
    // if (m_renderData.useNearestNeighbor) {
    //     tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //     tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // } else {
    //     tex->setTexParameter(GL_TEXTURE_MAG_FILTER, tex->magFilter);
    //     tex->setTexParameter(GL_TEXTURE_MIN_FILTER, tex->minFilter);
    // }

    drawRegionRects(m_renderData.damage, cb->vk());

    disableScissor();
}

SP<IRenderbuffer> CHyprVKRenderer::getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    return makeShared<CVKRenderBuffer>(buffer, fmt);
}

bool CHyprVKRenderer::beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple) {
    m_renderData.mainFB       = simple ? m_renderData.outFB : pMonitor->resources()->getUnusedWorkBuffer();
    m_renderData.currentFB    = m_renderData.mainFB;
    m_currentRenderbuffer     = dc<CVKFramebuffer*>(m_renderData.currentFB.get())->fb();
    m_currentRenderbufferSize = m_renderData.currentFB->m_size;
    m_currentRenderPass       = getRenderPass(m_currentDrmFormat);

    m_currentCommandBuffer = g_pHyprVulkan->begin();
    return true;
}

IHyprRenderer::eType CHyprVKRenderer::type() {
    return RT_VK;
}

void CHyprVKRenderer::startRenderPass() {
    m_currentCommandBuffer->useRenderPass(m_currentRenderPass);
    bindFB(m_currentRenderbuffer);

    m_inRenderPass = true;
}

void CHyprVKRenderer::bindFB(SP<CHyprVkFramebuffer> fb) {
    if (m_hasBoundFB == fb)
        return;

    if (!fb)
        return;

    if (m_hasBoundFB) {
        m_currentCommandBuffer->endRenderPass();
        if (m_hasBoundFB == dc<CVKFramebuffer*>(m_renderData.pMonitor->resources()->m_blurFB.get())->fb())
            m_currentCommandBuffer->changeLayout(m_hasBoundFB->texture()->m_image, //
                                                 {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                                                 {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
    }

    if (m_currentRenderPass->m_drmFormat != fb->texture()->m_drmFormat)
        m_currentRenderPass = getRenderPass(fb->texture()->m_drmFormat);

    startRenderPassHelper(m_currentRenderPass->m_vkRenderPass, fb->vk(), fb->texture()->m_size, m_currentCommandBuffer->vk());
    m_currentCommandBuffer->useTexture(fb->texture());
    m_hasBoundFB = fb;
}

void CHyprVKRenderer::bindFB(SP<IFramebuffer> fb) {
    bindFB(VKFB(fb)->fb());
}

UP<ISyncFDManager> CHyprVKRenderer::createSyncFDManager() {
    // TODO use vulkan sync
    return Render::GL::CEGLSync::create();
}

WP<IElementRenderer> CHyprVKRenderer::elementRenderer() {
    if UNLIKELY (!m_elementRenderer) {
        WP<IHyprRenderer> wpRenderer = g_pHyprRenderer;
        m_elementRenderer            = makeUnique<CVKElementRenderer>(dynamicPointerCast<CHyprVKRenderer>(wpRenderer));
    }
    return m_elementRenderer;
}

void CHyprVKRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    if (!m_inRenderPass)
        startRenderPass();

    if (m_renderData.pMonitor->m_transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        // FIXME
        m_renderData.damage = CBox{{0, 0}, m_renderData.pMonitor->m_transformedSize};
    }
    m_renderData.damage = m_renderPass.render(m_renderData.damage);

    g_pHyprVulkan->renderCB()->endRenderPass();
    m_inRenderPass = false;
    m_hasBoundFB.reset();

    if (m_renderData.outFB != m_renderData.mainFB) {
        m_currentCommandBuffer->changeLayout(VKFB(m_renderData.mainFB)->fb()->texture()->m_image, //
                                             {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                                             {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
        m_currentCommandBuffer->changeLayout(
            VKFB(m_renderData.outFB)->fb()->texture()->m_image, //
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
        m_renderData.outFB->bind();
        m_renderData.mainFB->getTexture()->m_transform = Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform));
        draw(
            CTexPassElement::SRenderData{
                .tex        = m_renderData.mainFB->getTexture(),
                .box        = {{0, 0}, m_renderData.pMonitor->m_transformedSize},
                .clipBox    = CBox{{0, 0}, m_renderData.pMonitor->m_transformedSize},
                .clipRegion = CBox{{0, 0}, m_renderData.pMonitor->m_transformedSize},
            },
            CBox{{0, 0}, m_renderData.mainFB->getTexture()->m_size});
        g_pHyprVulkan->renderCB()->endRenderPass();
        m_hasBoundFB.reset();
        m_currentRenderbuffer = VKFB(m_renderData.outFB)->fb();
        m_currentCommandBuffer->changeLayout(
            VKFB(m_renderData.mainFB)->fb()->texture()->m_image, //
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
    }

    std::vector<VkImageMemoryBarrier> acquireBarriers;
    std::vector<VkImageMemoryBarrier> releaseBarriers;
    acquireBarriers.reserve(m_needBarriers.size() + 1);
    releaseBarriers.reserve(m_needBarriers.size() + 1);

    for (const auto& tex : m_needBarriers) {
        acquireBarriers.push_back({
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
            .dstQueueFamilyIndex = g_pHyprVulkan->m_device->queueFamilyIndex(),
            .image               = VKTEX(tex)->m_image,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
        });

        releaseBarriers.push_back({
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask       = 0,
            .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = g_pHyprVulkan->m_device->queueFamilyIndex(),
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
            .image               = VKTEX(tex)->m_image,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
        });

        VKTEX(tex)->m_barriersSet = false;
    }
    m_needBarriers.clear();
    // TODO handle SHM tex sync

    VkImageLayout srcLayout = VK_IMAGE_LAYOUT_GENERAL;
    if (!m_currentRenderbuffer->m_initialized) {
        m_currentRenderbuffer->m_initialized = true;
        srcLayout                            = VK_IMAGE_LAYOUT_PREINITIALIZED;
    }

    acquireBarriers.push_back({
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout           = srcLayout,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .dstQueueFamilyIndex = g_pHyprVulkan->m_device->queueFamilyIndex(),
        .image               = m_currentRenderbuffer->vkImage(),
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    });

    releaseBarriers.push_back({
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask       = 0,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = g_pHyprVulkan->m_device->queueFamilyIndex(),
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
        .image               = m_currentRenderbuffer->vkImage(),
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    });

    vkCmdPipelineBarrier(g_pHyprVulkan->stageCB()->vk(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, acquireBarriers.size(), acquireBarriers.data());

    vkCmdPipelineBarrier(g_pHyprVulkan->renderCB()->vk(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                         releaseBarriers.size(), releaseBarriers.data());

    if (m_renderMode != RENDER_MODE_NORMAL)
        m_currentCommandBuffer->changeLayout(m_currentRenderbuffer->vkImage(),
                                             {
                                                 .layout     = VK_IMAGE_LAYOUT_GENERAL,
                                                 .stageMask  = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                                 .accessMask = VK_ACCESS_SHADER_READ_BIT,
                                             },
                                             {
                                                 .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                 .stageMask  = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                                 .accessMask = VK_ACCESS_SHADER_READ_BIT,
                                             });

    g_pHyprVulkan->end();

    if (m_renderMode == RENDER_MODE_NORMAL)
        renderData().pMonitor->m_output->state->setBuffer(m_currentBuffer);

    if (m_renderMode == RENDER_MODE_FULL_FAKE) {
        if (renderingDoneCallback)
            renderingDoneCallback();
    } else if (!explicitSyncSupported()) { // FIXME impossible with vulkan?
        Log::logger->log(Log::ERR, "renderer: Explicit sync unsupported, falling back to implicit in endRender");

        m_usedAsyncBuffers.clear(); // release all buffer refs and hope implicit sync works
        if (renderingDoneCallback)
            renderingDoneCallback();
    } else {
        auto sync = createSyncFDManager();
        if LIKELY (sync && sync->isValid()) {
            for (auto const& buf : m_usedAsyncBuffers) {
                for (const auto& releaser : buf->m_syncReleasers) {
                    releaser->addSyncFileFd(sync->fd());
                }
            }

            // release buffer refs with release points now, since syncReleaser handles actual buffer release based on EGLSync
            std::erase_if(m_usedAsyncBuffers, [](const auto& buf) { return !buf->m_syncReleasers.empty(); });

            // release buffer refs without release points when EGLSync sync_file/fence is signalled
            g_pEventLoopManager->doOnReadable(sync->fd().duplicate(), [renderingDoneCallback, prevbfs = std::move(m_usedAsyncBuffers)]() mutable {
                prevbfs.clear();
                if (renderingDoneCallback)
                    renderingDoneCallback();
            });
            m_usedAsyncBuffers.clear();

            if (m_renderMode == RENDER_MODE_NORMAL) {
                m_renderData.pMonitor->m_inFence = sync->takeFd();
                m_renderData.pMonitor->m_output->state->setExplicitInFence(m_renderData.pMonitor->m_inFence.get());
            }
        } else {
            Log::logger->log(Log::ERR, "renderer: Explicit sync failed, releasing resources");

            m_usedAsyncBuffers.clear(); // release all buffer refs and hope implicit sync works
            if (renderingDoneCallback)
                renderingDoneCallback();
        }
    }

    m_currentBuffer        = nullptr;
    m_renderData.currentFB = nullptr;
    m_renderData.mainFB    = nullptr;
    m_renderData.outFB     = nullptr;
    m_currentRenderPass.reset();
    m_busy = false;
}

SP<ITexture> CHyprVKRenderer::createStencilTexture(const int width, const int height) {
    // VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    return makeShared<CVKTexture>(VK_FORMAT_D24_UNORM_S8_UINT, width, height);
}

SP<ITexture> CHyprVKRenderer::createTexture(bool opaque) {
    return makeShared<CVKTexture>(opaque);
}

SP<ITexture> CHyprVKRenderer::createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) {
    auto tex = makeShared<CVKTexture>(drmFormat, pixels, stride, size, keepDataCopy, opaque);
    tex->m_lastUsedCB->useTexture(tex);
    return tex;
}

SP<ITexture> CHyprVKRenderer::createTexture(const Aquamarine::SDMABUFAttrs& attrs, bool opaque) {
    auto tex = makeShared<CVKTexture>(attrs, opaque);
    return tex;
}

SP<ITexture> CHyprVKRenderer::createTexture(const int width, const int height, unsigned char* const data) {
    auto tex = createTexture(DRM_FORMAT_ARGB8888, data, width * 4, {width, height});
    return tex;
}

SP<ITexture> CHyprVKRenderer::createTexture(cairo_surface_t* cairo) {
    const auto CAIROFORMAT = cairo_image_surface_get_format(cairo);

    const auto width  = cairo_image_surface_get_width(cairo);
    const auto height = cairo_image_surface_get_height(cairo);

    if (CAIROFORMAT == CAIRO_FORMAT_RGB96F) {
        const auto recodeSurf  = cairo_image_surface_create(CAIRO_FORMAT_RGB30, width, height);
        const auto recodeCairo = cairo_create(recodeSurf);
        cairo_set_source_surface(recodeCairo, cairo, 0, 0);
        cairo_rectangle(recodeCairo, 0, 0, width, height);
        cairo_set_operator(recodeCairo, CAIRO_OPERATOR_SOURCE);
        cairo_fill(recodeCairo);
        cairo_surface_flush(recodeSurf);

        auto tex = createTexture(DRM_FORMAT_XRGB2101010, cairo_image_surface_get_data(recodeSurf), width * 4, {width, height});

        cairo_surface_destroy(recodeSurf);
        cairo_destroy(recodeCairo);

        return tex;
    } else {
        const auto DATA = cairo_image_surface_get_data(cairo);
        return createTexture(width, height, DATA);
    }
}

SP<ITexture> CHyprVKRenderer::createTexture(std::span<const float> lut3D, size_t N) {
    Log::logger->log(Log::ERR, "fixme: unimplemented CHyprVKRenderer::createTexture for ICC lut");
    return nullptr;
}

bool CHyprVKRenderer::explicitSyncSupported() {
    return true; // FIXME actual check
}

std::vector<SDRMFormat> CHyprVKRenderer::getDRMFormats() {
    // TODO cache
    const auto              source = g_pHyprVulkan->m_device->formats();
    std::vector<SDRMFormat> formats(source.size());
    std::ranges::transform(source, formats.begin(), [](const auto fmt) {
        std::set<uint64_t> modSet;
        for (const auto& info : fmt.dmabuf.renderModifiers) {
            modSet.insert(info.props.drmFormatModifier);
        }
        for (const auto& info : fmt.dmabuf.textureModifiers) {
            modSet.insert(info.props.drmFormatModifier);
        }
        std::vector<uint64_t> mods(modSet.begin(), modSet.end());

        return SDRMFormat{
            .drmFormat = fmt.format.drmFormat,
            .modifiers = mods,
        };
    });
    return formats;
}

std::vector<uint64_t> CHyprVKRenderer::getDRMFormatModifiers(DRMFormat format) {
    auto fmt = g_pHyprVulkan->m_device->getFormat(format);
    if (!fmt.has_value())
        return {};

    std::set<uint64_t> modSet;
    for (const auto& info : fmt->dmabuf.renderModifiers) {
        modSet.insert(info.props.drmFormatModifier);
    }
    for (const auto& info : fmt->dmabuf.textureModifiers) {
        modSet.insert(info.props.drmFormatModifier);
    }
    std::vector<uint64_t> mods(modSet.begin(), modSet.end());
    return mods;
}

SP<IFramebuffer> CHyprVKRenderer::createFB(const std::string& name) {
    return makeShared<CVKFramebuffer>(name);
}

void CHyprVKRenderer::disableScissor() {
    VkRect2D rect = {
        .offset = {.x = 0, .y = 0},
        .extent = {.width = currentRBSize().x, .height = currentRBSize().y},
    };

    vkCmdSetScissor(g_pHyprVulkan->renderCB()->vk(), 0, 1, &rect);
}

void CHyprVKRenderer::blend(bool enabled) {
    if (enabled)
        return;
    static int count = 0;
    if (count < 10) {
        count++;
        Log::logger->log(Log::WARN, "Unimplimented blend");
    }
}

void CHyprVKRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) {
    RASSERT(m_renderData.pMonitor, "Tried to render shadow without begin()!");
    RASSERT((box.width > 0 && box.height > 0), "Tried to render shadow with width/height < 0!");

    if (m_renderData.damage.empty())
        return;

    TRACY_GPU_ZONE("RenderShadow");

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    static auto               PSHADOWPOWER = CConfigValue<Hyprlang::INT>("decoration:shadow:render_power");
    const auto                SHADOWPOWER  = std::clamp(sc<int>(*PSHADOWPOWER), 1, 4);
    const auto                TOPLEFT      = Vector2D(range + round, range + round);
    const auto                BOTTOMRIGHT  = Vector2D(newBox.width - (range + round), newBox.height - (range + round));
    const auto                FULLSIZE     = Vector2D(newBox.width, newBox.height);

    const auto&               vertData = matToVertShader(projectBoxToTarget(newBox).getMatrix());
    const SVkShadowShaderData fragData = {
        .color       = {color.r, color.g, color.b, color.a * a},
        .bottomRight = {BOTTOMRIGHT.x, BOTTOMRIGHT.y},
        .range       = range,
        .shadowPower = SHADOWPOWER,
    };
    uint8_t   usedFeatures = 0;
    SRounding roundingData = {
        .radius   = range + round,
        .power    = roundingPower,
        .topLeft  = {TOPLEFT.x, TOPLEFT.y},
        .fullSize = {FULLSIZE.x, FULLSIZE.y},
    };
    if (roundingData.radius > 0)
        usedFeatures |= SH_FEAT_ROUNDING;

    static const auto      PCM    = CConfigValue<Hyprlang::INT>("render:cm_enabled");
    const bool             skipCM = !*PCM || m_renderData.pMonitor->m_imageDescription->id() == NColorManagement::DEFAULT_IMAGE_DESCRIPTION->id();
    SShaderCM              cmData;
    SShaderTonemap         tonemapData;
    SShaderTargetPrimaries primariesData;
    if (!skipCM) {
        usedFeatures |= SH_FEAT_CM;
        auto settings = getCMSettings(m_renderData.pMonitor->m_imageDescription, NColorManagement::DEFAULT_IMAGE_DESCRIPTION,
                                      m_renderData.surface.valid() ? m_renderData.surface.lock() : nullptr);
        usedFeatures |= passCMUniforms(settings, cmData, tonemapData, primariesData);
    }

    const auto cb       = g_pHyprVulkan->renderCB();
    const auto pipeline = m_currentRenderPass->shadowPipeline(usedFeatures);
    const auto layout   = pipeline->layout().lock();

    bindPipeline(pipeline);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);
    int featuresOffset = sizeof(vertData) + sizeof(fragData);
    if (usedFeatures & SH_FEAT_ROUNDING) {
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, featuresOffset, sizeof(roundingData), &roundingData);
        featuresOffset += sizeof(roundingData);
    }
    if (usedFeatures & SH_FEAT_CM) {
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, featuresOffset, sizeof(cmData), &cmData);
        featuresOffset += sizeof(cmData);
    }
    if (usedFeatures & SH_FEAT_TONEMAP) {
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, featuresOffset, sizeof(tonemapData), &tonemapData);
        featuresOffset += sizeof(tonemapData);
    }
    if (usedFeatures & SH_FEAT_TONEMAP || usedFeatures & SH_FEAT_SDR_MOD) {
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, featuresOffset, sizeof(primariesData), &primariesData);
        featuresOffset += sizeof(primariesData);
    }

    CRegion damageClip{m_renderData.clipBox.x, m_renderData.clipBox.y, m_renderData.clipBox.width, m_renderData.clipBox.height};
    auto    clipBox = m_renderData.clipBox.width != 0 && m_renderData.clipBox.height != 0 ? damageClip.intersect(m_renderData.damage) : m_renderData.damage;

    drawRegionRects(clipBox, cb->vk());
}

void CHyprVKRenderer::startRenderPassHelper(VkRenderPass renderPass, VkFramebuffer fb, const Vector2D& size, VkCommandBuffer cb, int x, int y) {
    VkClearValue          clear[2] = {{}, {.depthStencil = {0, 0}}};
    VkRenderPassBeginInfo info     = {
            .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass  = renderPass,
            .framebuffer = fb,
            .renderArea =
            {
                    .offset = {x, y},
                    .extent = {size.x, size.y},
            },
            .clearValueCount = 2,
            .pClearValues    = clear,
    };
    VkSubpassBeginInfo subInfo = {
        .sType    = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
        .contents = VK_SUBPASS_CONTENTS_INLINE,
    };

    vkCmdBeginRenderPass2(cb, &info, &subInfo);

    VkViewport viewport = {
        .x        = x,
        .y        = y,
        .width    = size.x,
        .height   = size.y,
        .maxDepth = 1,
    };
    vkCmdSetViewport(cb, 0, 1, &viewport);

    m_viewport = {x, y, size.x, size.y};
}

void CHyprVKRenderer::setViewport(int x, int y, int width, int height) {
    if (m_hasBoundFB && m_viewport.x == x && m_viewport.y == y && m_viewport.width == width && m_viewport.height == height)
        return;

    if (m_hasBoundFB)
        m_currentCommandBuffer->endRenderPass();
    else
        m_hasBoundFB = m_currentRenderbuffer;

    startRenderPassHelper(m_currentRenderPass->m_vkRenderPass, m_hasBoundFB->vk(), {width, height}, m_currentCommandBuffer->vk(), x, y);
}

bool CHyprVKRenderer::reloadShaders(const std::string& path) {
    try {
        auto shaders = makeShared<CVkShaders>(g_pHyprVulkan->m_device);

        if (!shaders)
            return false;

        m_shaders = shaders;
        m_renderPassList.clear();
        m_blurPassList.clear();
        return true;
    } catch (const std::exception& e) {
        Log::logger->log(Log::ERR, "Shaders update failed: {}", e.what());
        return false;
    }
}

void CHyprVKRenderer::bindPipeline(WP<CVkPipeline> pipeline) {
    if (m_currentPipeline == pipeline)
        return;

    vkCmdBindPipeline(g_pHyprVulkan->renderCB()->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vk());
    m_currentPipeline = pipeline;
}

Vector2D CHyprVKRenderer::currentRBSize() {
    return m_currentBuffer && m_currentBuffer->dmabuf().size.x > 0 && m_currentBuffer->dmabuf().size.y > 0 ? m_currentBuffer->dmabuf().size : m_currentRenderbufferSize;
}

SP<CVKBorderGradientUBO> CHyprVKRenderer::getGradientForWindow(PHLWINDOWREF window) {
    std::erase_if(m_borderGradients, [](const auto& it) { return !it.first; });
    if (!window)
        return nullptr;

    if (m_borderGradients.contains(window))
        return m_borderGradients[window];

    auto gradient             = makeShared<CVKBorderGradientUBO>(g_pHyprVulkan->device(), m_currentRenderPass->borderPipeline()->layout()->descriptorSets()[0]);
    m_borderGradients[window] = gradient;
    return gradient;
}

SP<CVkPipelineLayout> CHyprVKRenderer::ensurePipelineLayout(CVkPipelineLayout::KEY key) {
    const auto pipeline = std::ranges::find_if(m_pipelineLayouts, [key](const auto layout) { return layout->key() == key; });
    if (pipeline != m_pipelineLayouts.end()) {
        return *pipeline;
    }

    return m_pipelineLayouts.emplace_back(makeShared<CVkPipelineLayout>(g_pHyprVulkan->m_device, key));
}

SP<CVkPipelineLayout> CHyprVKRenderer::ensurePipelineLayout(uint32_t vertSize, uint32_t fragSize, uint8_t texCount, uint8_t uboCount) {
    return ensurePipelineLayout({vertSize, fragSize, VK_FILTER_LINEAR, texCount, uboCount});
}

void CHyprVKRenderer::setTexBarriers(SP<ITexture> tex) {
    if (VKTEX(tex)->m_barriersSet)
        return;
    VKTEX(tex)->m_barriersSet = true;
    m_needBarriers.push_back(tex);
}