#include "VKRenderer.hpp"
#include "./vulkan/Framebuffer.hpp"
#include "./vulkan/Pipeline.hpp"
#include "./vulkan/RenderPass.hpp"
#include "./vulkan/Vulkan.hpp"
#include "./vulkan/PipelineLayout.hpp"
#include "./vulkan/Shaders.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/view/Window.hpp"
#include "render/Renderer.hpp"
#include "render/pass/PassElement.hpp"
#include "render/vulkan/VKTexture.hpp"
#include "render/vulkan/types.hpp"
#include <algorithm>
#include <cairo.h>
#include <cstdint>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Mat3x3.hpp>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

CHyprVKRenderer::CHyprVKRenderer() : IHyprRenderer() {
    if (!m_shaders)
        m_shaders = makeShared<CVkShaders>(g_pHyprVulkan->m_device);
}

void CHyprVKRenderer::initRender() {
    m_currentPipeline.reset();
}

bool CHyprVKRenderer::initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    auto foundRP = std::ranges::find_if(m_renderPassList, [&](const auto& other) { return other->m_drmFormat == fmt; });
    if (foundRP != m_renderPassList.end())
        m_currentRenderPass = *foundRP;
    else
        m_currentRenderPass = m_renderPassList.emplace_back(makeShared<CVkRenderPass>(g_pHyprVulkan->m_device, fmt, m_shaders));

    auto foundFB = std::ranges::find_if(m_renderBuffers, [&](const auto& other) { return other->m_hlBuffer == buffer; });
    if (foundFB != m_renderBuffers.end())
        m_currentRenderbuffer = *foundFB;
    else
        m_currentRenderbuffer = m_renderBuffers.emplace_back(makeShared<CHyprVkFramebuffer>(g_pHyprVulkan->m_device, buffer, m_currentRenderPass->m_vkRenderPass));

    return true;
};

bool CHyprVKRenderer::beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple) {
    m_currentCommandBuffer = g_pHyprVulkan->begin();
    return true;
}

bool CHyprVKRenderer::beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple) {
    return false;
};

void CHyprVKRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    const auto attrs = m_currentBuffer->dmabuf();

    m_renderPass.render(CRegion{CBox{{}, {INT32_MAX, INT32_MAX}}}, PS_INIT);

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

    vkCmdBeginRenderPass(m_currentCommandBuffer->vk(), &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {
        .width    = attrs.size.x,
        .height   = attrs.size.y,
        .maxDepth = 1,
    };
    vkCmdSetViewport(m_currentCommandBuffer->vk(), 0, 1, &viewport);

    const auto damage = m_renderPass.render(CRegion{CBox{{}, {INT32_MAX, INT32_MAX}}}, PS_MAIN);

    vkCmdEndRenderPass(g_pHyprVulkan->renderCB()->vk());

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
        .image               = m_currentRenderbuffer->m_image,
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
        .image               = m_currentRenderbuffer->m_image,
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

    g_pHyprVulkan->end();

    if (m_renderMode == RENDER_MODE_NORMAL)
        renderData().pMonitor->m_output->state->setBuffer(m_currentBuffer);

    if (renderingDoneCallback)
        renderingDoneCallback();

    m_currentBuffer = nullptr;
}

SP<ITexture> CHyprVKRenderer::createTexture(bool opaque) {
    // return makeShared<CVKTexture>(opaque);
    Log::logger->log(Log::WARN, "CHyprVKRenderer::createTexture");
    return nullptr;
}

SP<ITexture> CHyprVKRenderer::createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) {
    auto tex = makeShared<CVKTexture>(drmFormat, pixels, stride, size, keepDataCopy, opaque);
    tex->m_lastUsedCB->useTexture(tex);
    return tex;
}

SP<ITexture> CHyprVKRenderer::createTexture(const Aquamarine::SDMABUFAttrs& attrs, void* image, bool opaque) {
    // return makeShared<CVKTexture>(attrs, image, opaque);
    Log::logger->log(Log::WARN, "CHyprVKRenderer::createTexture dma");
    return nullptr;
}

SP<ITexture> CHyprVKRenderer::createTexture(const int width, const int height, unsigned char* const data) {
    return createTexture(DRM_FORMAT_ARGB8888, data, width * 4, {width, height});
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

void* CHyprVKRenderer::createImage(const SP<Aquamarine::IBuffer> buffer) {
    Log::logger->log(Log::WARN, "Unimplimented CHyprVKRenderer::createImage");
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

SP<IFramebuffer> CHyprVKRenderer::createFB() {
    Log::logger->log(Log::ERR, "Unimplimented CHyprVKRenderer::createFB");
    return nullptr;
}

void CHyprVKRenderer::draw(CBorderPassElement* element, const CRegion& damage) {
    // TODO
    // if (m_renderData.damage.empty())
    //     return;

    auto data   = element->m_data;
    CBox newBox = data.box;

    // TODO
    // m_renderData.renderModif.applyToBox(newBox);

    if (data.borderSize < 1)
        return;

    int scaledBorderSize = std::round(data.borderSize * m_renderData.pMonitor->m_scale);
    // TODO
    // scaledBorderSize     = std::round(scaledBorderSize * m_renderData.renderModif.combinedScale());

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

    // Log::logger->log(Log::DEBUG, "Border {}x{}@{}x{} -> {}x{}@{}x{}", data.box.width, data.box.height, data.box.x, data.box.y, transformedBox.width, transformedBox.height,
    //                  transformedBox.x, transformedBox.y);

    const auto        TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto        FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    const auto        cb = g_pHyprVulkan->renderCB();

    auto              projection = g_pHyprRenderer->m_renderData.pMonitor->m_projMatrix.projectBox(element->m_data.box, HYPRUTILS_TRANSFORM_NORMAL, element->m_data.box.rot);
    const auto        mat        = Mat3x3::outputProjection(g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize, HYPRUTILS_TRANSFORM_NORMAL).multiply(projection).getMatrix();

    SVkVertShaderData vertData = {
        .mat4 =
            {
                {mat[0], mat[1], 0, mat[2]},
                {mat[3], mat[4], 0, mat[5]},
                {0, 0, 1, 0},
                {0, 0, 0, 1},
            },
        .uvOffset = {0, 0},
        .uvSize   = {1, 1},
    };

    const auto layout = m_currentRenderPass->borderPipeline()->layout().lock();

    // TODO large gradients
    // const auto          gradientLength  = data.grad1.m_colorsOkLabA.size() / 4;
    // const auto          gradient2Length = data.grad2.m_colorsOkLabA.size() / 4;
    const auto          gradientLength  = data.grad1.m_colorsOkLabA.size() / 4 > 0 ? 1 : 0;
    const auto          gradient2Length = data.grad2.m_colorsOkLabA.size() / 4 > 0 ? 1 : 0;

    SVkBorderShaderData fragData = {
        .fullSizeUntransformed = {sc<float>(newBox.width), sc<float>(newBox.height)},
        .radiusOuter           = data.outerRound == -1 ? round : data.outerRound,
        .thick                 = scaledBorderSize,
        .gradientLength        = gradientLength,
        .gradient2Length       = gradient2Length,
        .angle                 = sc<int>(data.grad1.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0),
        .angle2                = sc<int>(data.grad2.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0),
        .gradientLerp          = data.lerp,
        .alpha                 = data.a,
        .radius                = round,
        .power                 = data.roundingPower,
        .topLeft               = {sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y)},
        .fullSize              = {sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y)},
    };

    for (unsigned i = 0; i < gradientLength; i++) {
        for (unsigned j = 0; j < 4; j++) {
            fragData.gradient[i][j] = data.grad1.m_colorsOkLabA.at(i * 4 + j);
        }
    }

    for (unsigned i = 0; i < gradient2Length; i++) {
        for (unsigned j = 0; j < 4; j++) {
            fragData.gradient2[i][j] = data.grad2.m_colorsOkLabA.at(i * 4 + j);
        }
    }

    bindPipeline(m_currentRenderPass->borderPipeline());

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

    // const auto clipBox = !element->m_data.clipBox.empty() ? element->m_data.clipBox : (element->m_data.damage.empty() ? damage : element->m_data.damage);
    // element->m_data.box.forEachRect([cb](const auto& RECT) {
    VkRect2D rect = {
        .offset = {.x = element->m_data.box.x, .y = element->m_data.box.y},
        .extent = {.width = element->m_data.box.width, .height = element->m_data.box.height},
    };

    vkCmdSetScissor(cb->vk(), 0, 1, &rect);
    vkCmdDraw(cb->vk(), 4, 1, 0, 0);
    // });
};

void CHyprVKRenderer::draw(CClearPassElement* element, const CRegion& damage) {
    VkClearColorValue clearValue;
    clearValue = {{element->m_data.color.r, element->m_data.color.g, element->m_data.color.b, element->m_data.color.a}};

    VkImageSubresourceRange clearRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };

    vkCmdClearColorImage(g_pHyprVulkan->renderCB()->vk(), m_currentRenderbuffer->m_image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
};

void CHyprVKRenderer::draw(CFramebufferElement* element, const CRegion& damage) {
    Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
};

void CHyprVKRenderer::draw(CPreBlurElement* element, const CRegion& damage) {
    static int count = 0;
    if (count < 10) {
        count++;
        Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
    }
};

void CHyprVKRenderer::draw(CRectPassElement* element, const CRegion& damage) {
    Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
};

void CHyprVKRenderer::draw(CRendererHintsPassElement* element, const CRegion& damage) {
    Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
};

void CHyprVKRenderer::draw(CShadowPassElement* element, const CRegion& damage) {
    // Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
};

void CHyprVKRenderer::draw(CSurfacePassElement* element, const CRegion& damage) {
    const auto cb = g_pHyprVulkan->renderCB();
    cb->useTexture(element->m_data.texture);
    const auto texture = dc<CVKTexture*>(element->m_data.texture.get());

    if (texture->isDMA())
        Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");

    CBox              box        = CBox{element->m_data.pos * g_pHyprRenderer->m_renderData.pMonitor->m_scale, texture->m_size};
    auto              projection = g_pHyprRenderer->m_renderData.pMonitor->m_projMatrix.projectBox(box, texture->m_transform, box.rot);
    const auto        mat        = Mat3x3::outputProjection(g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize, HYPRUTILS_TRANSFORM_NORMAL).multiply(projection).getMatrix();

    SVkVertShaderData vertData = {
        .mat4 =
            {
                {mat[0], mat[1], 0, mat[2]},
                {mat[3], mat[4], 0, mat[5]},
                {0, 0, 1, 0},
                {0, 0, 0, 1},
            },
        .uvOffset = {0, 0},
        .uvSize   = {1, 1},
    };

    const auto layout = m_currentRenderPass->texturePipeline()->layout().lock();
    const auto view   = texture->getView(layout);
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
        .alpha               = element->m_data.alpha,
        .luminanceMultiplier = 1.0,
    };

    bindPipeline(m_currentRenderPass->texturePipeline());

    const auto ds = view->vkDS();
    vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

    const auto clipBox = !element->m_data.clipBox.empty() ? element->m_data.clipBox : damage;
    // const CRegion clipBox = box;
    clipBox.forEachRect([cb](const auto& RECT) {
        VkRect2D rect = {
            .offset = {.x = RECT.x1, .y = RECT.y1},
            .extent = {.width = RECT.x2 - RECT.x1, .height = RECT.y2 - RECT.y1},
        };

        vkCmdSetScissor(cb->vk(), 0, 1, &rect);
        vkCmdDraw(cb->vk(), 4, 1, 0, 0);
    });

    texture->m_lastUsedCB = cb;

    if (texture->isDMA())
        Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");
};

void CHyprVKRenderer::draw(CTexPassElement* element, const CRegion& damage) {
    const auto cb = g_pHyprVulkan->renderCB();
    cb->useTexture(element->m_data.tex);
    const auto texture = dc<CVKTexture*>(element->m_data.tex.get());

    if (texture->isDMA())
        Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");

    auto projection = element->m_data.replaceProjection.value_or(g_pHyprRenderer->m_renderData.pMonitor->m_projMatrix)
                          .projectBox(element->m_data.box, texture->m_transform, element->m_data.box.rot);
    const auto        mat = Mat3x3::outputProjection(g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize, HYPRUTILS_TRANSFORM_NORMAL).multiply(projection).getMatrix();

    SVkVertShaderData vertData = {
        .mat4 =
            {
                {mat[0], mat[1], 0, mat[2]},
                {mat[3], mat[4], 0, mat[5]},
                {0, 0, 1, 0},
                {0, 0, 0, 1},
            },
        .uvOffset = {0, 0},
        .uvSize   = {1, 1},
    };

    const auto layout = m_currentRenderPass->texturePipeline()->layout().lock();
    const auto view   = texture->getView(layout);
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

    const auto clipBox = !element->m_data.clipBox.empty() ? element->m_data.clipBox : (element->m_data.damage.empty() ? damage : element->m_data.damage);
    clipBox.forEachRect([cb](const auto& RECT) {
        VkRect2D rect = {
            .offset = {.x = RECT.x1, .y = RECT.y1},
            .extent = {.width = RECT.x2 - RECT.x1, .height = RECT.y2 - RECT.y1},
        };

        vkCmdSetScissor(cb->vk(), 0, 1, &rect);
        vkCmdDraw(cb->vk(), 4, 1, 0, 0);
    });

    texture->m_lastUsedCB = cb;

    if (texture->isDMA())
        Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");
};

void CHyprVKRenderer::draw(CTextureMatteElement* element, const CRegion& damage) {
    Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
};

void CHyprVKRenderer::bindPipeline(WP<CVkPipeline> pipeline) {
    if (m_currentPipeline == pipeline)
        return;

    vkCmdBindPipeline(g_pHyprVulkan->renderCB()->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vk());
    m_currentPipeline = pipeline;
}

SP<CVkPipelineLayout> CHyprVKRenderer::ensurePipelineLayout(CVkPipelineLayout::KEY key) {
    const auto pipeline = std::ranges::find_if(m_pipelineLayouts, [key](const auto layout) { return layout->key() == key; });
    if (pipeline != m_pipelineLayouts.end()) {
        return *pipeline;
    }

    return m_pipelineLayouts.emplace_back(makeShared<CVkPipelineLayout>(g_pHyprVulkan->m_device, key));
}

SP<CVkPipelineLayout> CHyprVKRenderer::ensurePipelineLayout(uint32_t vertSize, uint32_t fragSize) {
    return ensurePipelineLayout({vertSize, fragSize, VK_FILTER_LINEAR});
}
