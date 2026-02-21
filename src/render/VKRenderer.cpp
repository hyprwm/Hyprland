#include "VKRenderer.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/Pipeline.hpp"
#include "./vulkan/RenderPass.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/PipelineLayout.hpp"
#include "./vulkan/Shaders.hpp"
#include "./vulkan/utils.hpp"
#include "debug/log/Logger.hpp"
#include "macros.hpp"
#include "render/Framebuffer.hpp"
#include "render/OpenGL.hpp"
#include "render/Renderer.hpp"
#include "render/decorations/CHyprDropShadowDecoration.hpp"
#include "render/pass/PassElement.hpp"
#include "render/vulkan/BlurPass.hpp"
#include "render/vulkan/BorderGradientUBO.hpp"
#include "render/vulkan/VKTexture.hpp"
#include "render/vulkan/types.hpp"
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
#include <vector>
#include <vulkan/vulkan_core.h>

CHyprVKRenderer::CHyprVKRenderer() : IHyprRenderer() {
    if (!m_shaders)
        m_shaders = makeShared<CVkShaders>(g_pHyprVulkan->m_device);
}

void CHyprVKRenderer::initRender() {
    ASSERT(!m_busy);
    m_busy = true;
    m_currentPipeline.reset();
    // for (const auto& cb : g_pHyprVulkan->m_commandBuffers) {
    //     if (!cb->busy()) {
    //         Log::logger->log(Log::DEBUG, "CHyprVKRenderer::initRender reset cb {}", cb->m_timelinePoint <= g_pHyprVulkan->m_device->timelinePoint());
    //         // cb->resetUsedResources();
    //     }
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
    m_currentRenderPass = getRenderPass(fmt);

    m_renderData.currentFB    = getOrCreateRenderbuffer(buffer, fmt)->getFB();
    m_currentRenderbuffer     = dc<CVKFramebuffer*>(m_renderData.currentFB.get())->fb();
    m_currentRenderbufferSize = m_renderData.currentFB->m_size;

    return true;
};

SP<ITexture> CHyprVKRenderer::blurFramebuffer(SP<IFramebuffer> source, float a, CRegion* originalDamage) {
    const auto bp = getBlurPass(m_currentRenderbuffer->texture()->m_drmFormat);
    if (m_hasBoundFB)
        vkCmdEndRenderPass(m_currentCommandBuffer->vk());

    m_currentCommandBuffer->changeLayout(dc<CVKTexture*>(source->getTexture().get())->m_image, //
                                         {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                                         {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    const auto tex = bp->blurTexture(source->getTexture(), m_renderData.pMonitor->m_mirrorFB, m_renderData.pMonitor->m_mirrorSwapFB);

    m_currentCommandBuffer->changeLayout(
        dc<CVKTexture*>(source->getTexture().get())->m_image, //
        {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
        {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    if (m_hasBoundFB)
        startRenderPassHelper(m_currentRenderPass->m_vkRenderPass, m_hasBoundFB->vk(), m_hasBoundFB->texture()->m_size, m_currentCommandBuffer->vk());
    return tex;
}

void CHyprVKRenderer::renderOffToMain(IFramebuffer* off) {
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
    const auto  view   = dc<CVKTexture*>(tex.get())->getView(layout);
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
    m_currentCommandBuffer = g_pHyprVulkan->begin();
    return true;
}

bool CHyprVKRenderer::beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple) {
    initRender();

    m_renderData.currentFB    = fb;
    m_currentRenderbuffer     = dc<CVKFramebuffer*>(fb.get())->fb();
    m_currentRenderbufferSize = fb->m_size;

    return beginRenderInternal(pMonitor, damage, simple);
};

void CHyprVKRenderer::startRenderPass() {
    // TODO damage
    m_renderPass.render(CRegion{CBox{{}, {INT32_MAX, INT32_MAX}}}, PS_INIT);

    bindFB(m_currentRenderbuffer);

    m_inRenderPass = true;
}

void CHyprVKRenderer::bindFB(SP<CHyprVkFramebuffer> fb) {
    if (m_hasBoundFB == fb)
        return;

    if (m_hasBoundFB) {
        vkCmdEndRenderPass(m_currentCommandBuffer->vk());
        if (m_hasBoundFB == dc<CVKFramebuffer*>(m_renderData.pMonitor->m_blurFB.get())->fb())
            m_currentCommandBuffer->changeLayout(m_hasBoundFB->texture()->m_image, //
                                                 {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                                                 {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
    }

    if (m_currentRenderPass->m_drmFormat != fb->texture()->m_drmFormat)
        m_currentRenderPass = getRenderPass(fb->texture()->m_drmFormat);

    startRenderPassHelper(m_currentRenderPass->m_vkRenderPass, fb->vk(), fb->texture()->m_size, m_currentCommandBuffer->vk());
    m_hasBoundFB = fb;
}

void CHyprVKRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    if (!m_inRenderPass)
        startRenderPass();

    const auto damage = m_renderPass.render(CRegion{CBox{{}, {INT32_MAX, INT32_MAX}}}, PS_MAIN);

    vkCmdEndRenderPass(g_pHyprVulkan->renderCB()->vk());
    m_inRenderPass = false;
    m_hasBoundFB.reset();

    std::vector<VkImageMemoryBarrier> acquireBarriers;
    std::vector<VkImageMemoryBarrier> releaseBarriers;

    // TODO handle DMA tex sync
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

    if (renderingDoneCallback)
        renderingDoneCallback();

    m_currentBuffer = nullptr;
    m_busy          = false;
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
    static int count = 0;
    if (count < 10) {
        count++;
        Log::logger->log(Log::WARN, "Unimplimented blend");
    }
}

void CHyprVKRenderer::draw(CBorderPassElement* element, const CRegion& damage) {
    if (m_renderData.damage.empty())
        return;

    auto data   = element->m_data;
    CBox newBox = data.box;
    m_renderData.renderModif.applyToBox(newBox);

    if (data.borderSize < 1)
        return;

    int scaledBorderSize = std::round(data.borderSize * m_renderData.pMonitor->m_scale);
    scaledBorderSize     = std::round(scaledBorderSize * m_renderData.renderModif.combinedScale());

    // adjust box
    newBox.x -= scaledBorderSize;
    newBox.y -= scaledBorderSize;
    newBox.width += 2 * scaledBorderSize;
    newBox.height += 2 * scaledBorderSize;

    float round = data.round + (data.round == 0 ? 0 : scaledBorderSize);

    // TODO handle transforms
    CBox transformedBox = newBox;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto  TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto  FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    const auto  cb = g_pHyprVulkan->renderCB();

    const auto& vertData = matToVertShader(projectBoxToTarget(newBox).getMatrix());

    // TODO CM
    const auto layout = m_currentRenderPass->borderPipeline()->layout().lock();

    // TODO large gradients
    const auto          gradientLength  = data.grad1.m_colorsOkLabA.size() / 4;
    const auto          gradient2Length = data.grad2.m_colorsOkLabA.size() / 4;

    SVkBorderShaderData fragData = {
        .fullSizeUntransformed = {sc<float>(element->m_data.box.width), sc<float>(element->m_data.box.height)},
        .radiusOuter           = data.outerRound == -1 ? round : data.outerRound,
        .thick                 = scaledBorderSize,
        .angle                 = sc<int>(data.grad1.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0),
        .angle2                = sc<int>(data.grad2.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0),
        .alpha                 = data.a,
        .rounding =
            {
                .radius   = round,
                .power    = data.roundingPower,
                .topLeft  = {sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y)},
                .fullSize = {sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y)},
            },
    };

    SVkBorderGradientShaderData gradientData = {
        .gradientLength  = gradientLength,
        .gradient2Length = gradient2Length,
        .gradientLerp    = data.lerp,
    };

    for (unsigned i = 0; i < gradientLength; i++) {
        for (unsigned j = 0; j < 4; j++) {
            gradientData.gradient[i][j] = data.grad1.m_colorsOkLabA.at(i * 4 + j);
        }
    }

    for (unsigned i = 0; i < gradient2Length; i++) {
        for (unsigned j = 0; j < 4; j++) {
            gradientData.gradient2[i][j] = data.grad2.m_colorsOkLabA.at(i * 4 + j);
        }
    }

    bindPipeline(m_currentRenderPass->borderPipeline());

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

    auto ubo = getGradientForWindow(data.window);
    if (ubo) {
        auto ds = ubo->ds();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);
        ubo->update(gradientData);
    }

    CRegion clipBox = m_renderData.damage.copy().intersect(newBox);
    clipBox.subtract(element->m_data.box.copy().expand(-scaledBorderSize - round));

    if (m_renderData.clipBox.width != 0 && m_renderData.clipBox.height != 0)
        clipBox.intersect(m_renderData.clipBox);

    drawRegionRects(clipBox, cb->vk());
};

void CHyprVKRenderer::draw(CClearPassElement* element, const CRegion& damage) {
    if (m_hasBoundFB) {
        const auto& dmg = damage.empty() ? m_renderData.damage : damage;
        if (dmg.empty())
            return;

        VkClearAttachment clearAttachment = {
            .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
            .colorAttachment = 0,
            .clearValue      = {.color = {{element->m_data.color.r, element->m_data.color.g, element->m_data.color.b, element->m_data.color.a}}},
        };

        std::vector<VkClearRect> rects;
        const CBox               max = {{0, 0}, currentRBSize()};
        m_renderData.damage.copy().intersect(max).forEachRect([&](const auto& RECT) {
            rects.push_back(VkClearRect{
                .rect =
                    {
                        .offset = {.x = RECT.x1, .y = RECT.y1},
                        .extent = {.width = RECT.x2 - RECT.x1, .height = RECT.y2 - RECT.y1},
                    },
                .baseArrayLayer = 0,
                .layerCount     = 1,
            });
        });

        vkCmdClearAttachments(g_pHyprVulkan->renderCB()->vk(), 1, &clearAttachment, rects.size(), rects.data());
        return;
    }

    VkClearColorValue clearValue;
    clearValue = {{element->m_data.color.r, element->m_data.color.g, element->m_data.color.b, element->m_data.color.a}};

    VkImageSubresourceRange clearRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };

    vkCmdClearColorImage(g_pHyprVulkan->renderCB()->vk(), m_currentRenderbuffer->vkImage(), VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
};

void CHyprVKRenderer::draw(CFramebufferElement* element, const CRegion& damage) {
    Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
};

void CHyprVKRenderer::draw(CPreBlurElement* element, const CRegion& damage) {
    auto dmg = damage;
    preBlurForCurrentMonitor(&dmg);
};

void CHyprVKRenderer::draw(CRectPassElement* element, const CRegion& damage) {
    const auto&             data = element->m_data;

    const auto&             vertData = matToVertShader(projectBoxToTarget(data.modifiedBox).getMatrix());
    const SVkRectShaderData fragData = {
        .color = {data.color.r * data.color.a, data.color.g * data.color.a, data.color.b * data.color.a, data.color.a},
        .rounding =
            {
                .radius   = data.round,
                .power    = data.roundingPower,
                .topLeft  = {data.TOPLEFT[0], data.TOPLEFT[1]},
                .fullSize = {data.FULLSIZE[0], data.FULLSIZE[1]},
            },
    };

    const auto cb       = g_pHyprVulkan->renderCB();
    const auto pipeline = m_currentRenderPass->rectPipeline();
    const auto layout   = pipeline->layout().lock();

    bindPipeline(pipeline);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

    drawRegionRects(data.drawRegion, cb->vk());
};

void CHyprVKRenderer::draw(CShadowPassElement* element, const CRegion& damage) {
    element->m_data.deco->render(m_renderData.pMonitor.lock(), element->m_data.a);
};

// void CHyprVKRenderer::draw(CSurfacePassElement* element, const CRegion& damage) {
//     const auto cb = g_pHyprVulkan->renderCB();
//     cb->useTexture(element->m_data.texture);
//     const auto texture = dc<CVKTexture*>(element->m_data.texture.get());

//     if (texture->isDMA())
//         Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");

//     CBox        box = CBox{element->m_data.pos * m_renderData.pMonitor->m_scale, texture->m_size};
//     const auto  mat = projectBoxToTarget(box, texture->m_transform).getMatrix();

//     const auto& vertData = matToVertShader(mat);

//     const auto  layout = m_currentRenderPass->texturePipeline()->layout().lock();
//     const auto  view   = texture->getView(layout);
//     if (!view)
//         return;

//     const auto        ctm = Mat3x3::identity().getMatrix();

//     SVkFragShaderData fragData = {
//         .matrix =
//             {
//                 {ctm[0], ctm[1], ctm[2], 0},
//                 {ctm[3], ctm[4], ctm[5], 0},
//                 {ctm[6], ctm[7], ctm[8], 0},
//                 {0, 0, 0, 0},
//             },
//         .alpha               = element->m_data.alpha,
//         .luminanceMultiplier = 1.0,
//     };

//     bindPipeline(m_currentRenderPass->texturePipeline());

//     const auto ds = view->vkDS();
//     vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

//     vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
//     vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

//     auto clipBox = !element->m_data.clipBox.empty() ? element->m_data.clipBox : damage;

//     drawRegionRects(clipBox, cb->vk());

//     texture->m_lastUsedCB = cb;

//     if (texture->isDMA())
//         Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");
// };

void CHyprVKRenderer::draw(CTexPassElement* element, const CRegion& damage) {
    // TODO
    // auto        discardOpacity = element->m_data.ignoreAlpha.has_value() ? *element->m_data.ignoreAlpha : element->m_data.discardOpacity;
    // auto        discardMode    = element->m_data.ignoreAlpha.has_value() ? DISCARD_ALPHA : element->m_data.discardMode;

    // const auto& data   = element->m_data;
    // float       alpha  = std::clamp(data.a, 0.f, 1.f);
    // const auto  box    = element->m_data.box;
    // CBox        newBox = box;
    // g_pHyprRenderer->m_renderData.renderModif.applyToBox(newBox);

    if (element->m_data.blur) {
        auto el = makeUnique<CTexPassElement>(CTexPassElement::SRenderData{
            .tex     = element->m_data.blurredBG,
            .box     = CBox{{0, 0}, m_renderData.pMonitor->m_transformedSize},
            .clipBox = CBox{element->m_data.box.pos(), element->m_data.tex->m_size},
        });
        draw(el.get(), damage);
    }

    const auto cb = g_pHyprVulkan->renderCB();
    cb->useTexture(element->m_data.tex);
    const auto texture = dc<CVKTexture*>(element->m_data.tex.get());

    if (texture->isDMA())
        Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");

    const auto  mat = projectBoxToTarget(element->m_data.box, texture->m_transform).getMatrix();

    const auto& vertData = matToVertShader(mat);

    const auto  layout = m_currentRenderPass->texturePipeline()->layout().lock();
    const auto  view   = texture->getView(layout);
    if (!view)
        return;

    const auto        ctm = Mat3x3::identity().getMatrix();

    SVkFragShaderData fragData = {
        .matrix =
            {
                {ctm[0], ctm[1], ctm[2], 0},
                {ctm[3], ctm[4], ctm[5], 0},
                {ctm[6], ctm[7], ctm[8], 0},
                {0, 0, 0, 0},
            },
        .alpha               = element->m_data.a,
        .luminanceMultiplier = 1.0,
    };

    bindPipeline(m_currentRenderPass->texturePipeline());

    const auto ds = view->vkDS();
    vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

    auto clipBox = !element->m_data.clipRegion.empty() ?
        element->m_data.clipRegion :
        (!element->m_data.clipBox.empty() ? element->m_data.clipBox : (element->m_data.damage.empty() ? damage : element->m_data.damage));

    drawRegionRects(clipBox, cb->vk());

    texture->m_lastUsedCB = cb;

    if (texture->isDMA())
        Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");
};

void CHyprVKRenderer::draw(CTextureMatteElement* element, const CRegion& damage) {
    static int count = 0;
    if (count < 10) {
        count++;
        Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
    }
    return;

    const auto cb       = g_pHyprVulkan->renderCB();
    const auto tex      = element->m_data.tex;
    const auto texMatte = element->m_data.fb->getTexture();
    cb->useTexture(tex);
    cb->useTexture(texMatte);
    RASSERT((tex->ok()), "Attempted to draw nullptr texture!");

    TRACY_GPU_ZONE("RenderTextureMatte");

    CBox newBox = element->m_data.box;
    m_renderData.renderModif.applyToBox(newBox);

    const auto  mat = projectBoxToTarget(element->m_data.box).getMatrix();

    const auto& vertData = matToVertShader(mat);

    const auto  layout = m_currentRenderPass->textureMattePipeline()->layout().lock();

    const auto  view = dc<CVKTexture*>(tex.get())->getView(layout);
    if (!view)
        return;

    const auto viewMatte = dc<CVKTexture*>(texMatte.get())->getView(layout);
    if (!viewMatte)
        return;

    bindPipeline(m_currentRenderPass->textureMattePipeline());

    const std::array<VkDescriptorSet, 2> ds = {view->vkDS(), viewMatte->vkDS()};
    vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, ds.size(), ds.data(), 0, nullptr);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);

    drawRegionRects(m_renderData.damage, cb->vk());
};

void CHyprVKRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) {
    RASSERT(m_renderData.pMonitor, "Tried to render shadow without begin()!");
    RASSERT((box.width > 0 && box.height > 0), "Tried to render shadow with width/height < 0!");

    if (m_renderData.damage.empty())
        return;

    TRACY_GPU_ZONE("RenderShadow");

    CBox newBox = box;
    m_renderData.renderModif.applyToBox(newBox);

    static auto PSHADOWPOWER = CConfigValue<Hyprlang::INT>("decoration:shadow:render_power");
    const auto  SHADOWPOWER  = std::clamp(sc<int>(*PSHADOWPOWER), 1, 4);
    const auto  TOPLEFT      = Vector2D(range + round, range + round);
    const auto  BOTTOMRIGHT  = Vector2D(newBox.width - (range + round), newBox.height - (range + round));
    const auto  FULLSIZE     = Vector2D(newBox.width, newBox.height);

    blend(true);

    const auto&               vertData = matToVertShader(projectBoxToTarget(newBox).getMatrix());
    const SVkShadowShaderData fragData = {
        .color       = {color.r, color.g, color.b, color.a * a},
        .bottomRight = {BOTTOMRIGHT.x, BOTTOMRIGHT.y},
        .range       = range,
        .shadowPower = SHADOWPOWER,
        .rounding =
            {
                .radius   = range + round,
                .power    = roundingPower,
                .topLeft  = {TOPLEFT.x, TOPLEFT.y},
                .fullSize = {FULLSIZE.x, FULLSIZE.y},
            },
    };

    const auto cb       = g_pHyprVulkan->renderCB();
    const auto pipeline = m_currentRenderPass->shadowPipeline();
    const auto layout   = pipeline->layout().lock();

    bindPipeline(pipeline);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

    CRegion damageClip{m_renderData.clipBox.x, m_renderData.clipBox.y, m_renderData.clipBox.width, m_renderData.clipBox.height};
    auto    clipBox = m_renderData.clipBox.width != 0 && m_renderData.clipBox.height != 0 ? damageClip.intersect(m_renderData.damage) : m_renderData.damage;

    clipBox = clipBox.subtract({newBox.x + range, newBox.y + range, newBox.width - range * 2, newBox.height - range * 2});

    drawRegionRects(clipBox, cb->vk());
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

    auto gradient             = makeShared<CVKBorderGradientUBO>(g_pHyprVulkan->device(), m_currentRenderPass->borderPipeline()->layout()->descriptorSet());
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
