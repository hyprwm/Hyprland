#include "VKElementRenderer.hpp"
#include "../Renderer.hpp"
#include "../decorations/CHyprDropShadowDecoration.hpp"
#include "Vulkan.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../protocols/ColorManagement.hpp"
#include "utils.hpp"
#include <cstdint>

using namespace Render::VK;

CVKElementRenderer::CVKElementRenderer(WP<CHyprVKRenderer> renderer) : m_renderer(renderer) {}

void CVKElementRenderer::draw(WP<CBorderPassElement> element, const CRegion& damage) {
    auto& m_renderData = m_renderer->m_renderData;
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

    const auto          TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto          FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    const auto          cb = g_pHyprVulkan->renderCB();

    const auto&         vertData = matToVertShader(m_renderer->projectBoxToTarget(newBox).getMatrix());

    const auto          gradientLength  = data.grad1.m_colorsOkLabA.size() / 4;
    const auto          gradient2Length = data.grad2.m_colorsOkLabA.size() / 4;

    SVkBorderShaderData fragData = {
        .fullSizeUntransformed = {sc<float>(element->m_data.box.width), sc<float>(element->m_data.box.height)},
        .radiusOuter           = data.outerRound == -1 ? round : data.outerRound,
        .thick                 = scaledBorderSize,
        .angle                 = sc<int>(data.grad1.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0),
        .angle2                = sc<int>(data.grad2.m_angle / (std::numbers::pi / 180.0)) % 360 * (std::numbers::pi / 180.0),
        .alpha                 = data.a,
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

    uint8_t   usedFeatures = 0;
    SRounding roundingData = {
        .radius   = round,
        .power    = data.roundingPower,
        .topLeft  = {sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y)},
        .fullSize = {sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y)},
    };
    if (roundingData.radius > 0)
        usedFeatures |= SH_FEAT_ROUNDING;

    static const auto      PCM    = CConfigValue<Hyprlang::INT>("render:cm_enabled");
    const bool             skipCM = !*PCM || m_renderData.pMonitor->m_imageDescription->id() == NColorManagement::getDefaultImageDescription()->id();
    SShaderCM              cmData;
    SShaderTonemap         tonemapData;
    SShaderTargetPrimaries primariesData;
    if (!skipCM) {
        usedFeatures |= SH_FEAT_CM;
        auto settings = m_renderer->getCMSettings(m_renderData.pMonitor->m_imageDescription, NColorManagement::getDefaultImageDescription(),
                                                  m_renderData.surface.valid() ? m_renderData.surface.lock() : nullptr);
        usedFeatures |= passCMUniforms(settings, cmData, tonemapData, primariesData);
    }

    auto       pipeline = m_renderer->m_currentRenderPass->borderPipeline(usedFeatures);
    const auto layout   = pipeline->layout().lock();

    m_renderer->bindPipeline(pipeline);

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

    auto ubo = m_renderer->getGradientForWindow(data.window);
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

void CVKElementRenderer::draw(WP<CClearPassElement> element, const CRegion& damage) {
    auto& m_renderData = m_renderer->m_renderData;
    if (m_renderer->m_hasBoundFB) {
        auto dmg = damage.empty() ? m_renderData.damage : damage;
        if (dmg.empty())
            return;

        VkClearAttachment clearAttachment = {
            .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
            .colorAttachment = 0,
            .clearValue      = {.color = {{element->m_data.color.r, element->m_data.color.g, element->m_data.color.b, element->m_data.color.a}}},
        };

        std::vector<VkClearRect> rects;
        const CBox               max = {{0, 0}, m_renderer->currentRBSize()};
        m_renderData.renderModif.applyToRegion(dmg);
        dmg.intersect(max).forEachRect([&](const auto& RECT) {
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

        if (rects.size())
            vkCmdClearAttachments(g_pHyprVulkan->renderCB()->vk(), 1, &clearAttachment, rects.size(), rects.data());
        // else {
        //     VkClearRect rect = {
        //         .rect =
        //             {
        //                 .offset = {.x = 0, .y = 0},
        //                 .extent = {.width = currentRBSize().x, .height = currentRBSize().y},
        //             },
        //         .baseArrayLayer = 0,
        //         .layerCount     = 1,
        //     };
        //     vkCmdClearAttachments(g_pHyprVulkan->renderCB()->vk(), 1, &clearAttachment, 1, &rect);
        // }
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

    vkCmdClearColorImage(g_pHyprVulkan->renderCB()->vk(), VKFB(m_renderData.currentFB)->fb()->vkImage(), VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
};

void CVKElementRenderer::draw(WP<CFramebufferElement> element, const CRegion& damage) {
    Log::logger->log(Log::ERR, "Deprecated CFramebufferElement. Use m_renderer->m_renderData and CTexPassElement instead");
};

void CVKElementRenderer::draw(WP<CPreBlurElement> element, const CRegion& damage) {
    auto dmg = damage;
    m_renderer->preBlurForCurrentMonitor(&dmg);
};

void CVKElementRenderer::draw(WP<CRectPassElement> element, const CRegion& damage) {
    const auto&             data = element->m_data;

    const auto&             vertData = matToVertShader(m_renderer->projectBoxToTarget(data.modifiedBox).getMatrix());
    const SVkRectShaderData fragData = {
        .color = {data.color.r * data.color.a, data.color.g * data.color.a, data.color.b * data.color.a, data.color.a},
    };
    uint8_t   usedFeatures = 0;
    SRounding roundingData = {
        .radius   = data.round,
        .power    = data.roundingPower,
        .topLeft  = {data.TOPLEFT[0], data.TOPLEFT[1]},
        .fullSize = {data.FULLSIZE[0], data.FULLSIZE[1]},

    };
    if (roundingData.radius > 0)
        usedFeatures |= SH_FEAT_ROUNDING;

    const auto cb       = g_pHyprVulkan->renderCB();
    const auto pipeline = m_renderer->m_currentRenderPass->rectPipeline(usedFeatures);
    const auto layout   = pipeline->layout().lock();

    m_renderer->bindPipeline(pipeline);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);
    if (usedFeatures & SH_FEAT_ROUNDING)
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData) + sizeof(fragData), sizeof(roundingData), &roundingData);

    drawRegionRects(data.drawRegion, cb->vk());
};

void CVKElementRenderer::draw(WP<CShadowPassElement> element, const CRegion& damage) {
    VK_CB_LABEL_BEGIN(m_renderer->m_currentCommandBuffer->vk(), "Shadow element")
    auto&            m_renderData        = m_renderer->m_renderData;
    static auto      PSHADOWIGNOREWINDOW = CConfigValue<Hyprlang::INT>("decoration:shadow:ignore_window");
    SP<IFramebuffer> mirrorFB;
    SP<IFramebuffer> mirrorSwapFB;
    if (*PSHADOWIGNOREWINDOW) {
        mirrorFB     = m_renderData.pMonitor->resources()->getUnusedWorkBuffer();
        mirrorSwapFB = m_renderData.pMonitor->resources()->getUnusedWorkBuffer();
        if (m_renderer->m_hasBoundFB)
            m_renderer->m_currentRenderPass->endRendering();
        m_renderer->m_currentCommandBuffer->changeLayout(
            mirrorFB, //
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
        m_renderer->m_currentCommandBuffer->changeLayout(
            mirrorSwapFB, //
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

        if (m_renderer->m_hasBoundFB)
            m_renderer->m_currentRenderPass->beginRendering(m_renderer->m_currentCommandBuffer.lock(), m_renderer->m_hasBoundFB);
    }

    element->m_data.deco->render(m_renderData.pMonitor.lock(), element->m_data.a);

    if (*PSHADOWIGNOREWINDOW) {
        if (m_renderer->m_hasBoundFB)
            m_renderer->m_currentRenderPass->endRendering();
        m_renderer->m_currentCommandBuffer->changeLayout(
            mirrorFB, //
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
        m_renderer->m_currentCommandBuffer->changeLayout(
            mirrorSwapFB, //
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

        if (m_renderer->m_hasBoundFB)
            m_renderer->m_currentRenderPass->beginRendering(m_renderer->m_currentCommandBuffer.lock(), m_renderer->m_hasBoundFB);
    }
    VK_CB_LABEL_END(m_renderer->m_currentCommandBuffer->vk())
};

void CVKElementRenderer::draw(WP<CInnerGlowPassElement> element, const CRegion& damage) {
    Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
}

void CVKElementRenderer::draw(WP<CTexPassElement> element, const CRegion& damage) {
    auto&             m_renderData = m_renderer->m_renderData;
    static const auto PCM          = CConfigValue<Hyprlang::INT>("render:cm_enabled");
    const auto        sdrEOTF      = NTransferFunction::fromConfig();

    auto              discardOpacity = element->m_data.ignoreAlpha.has_value() ? *element->m_data.ignoreAlpha : element->m_data.discardOpacity;
    auto              discardMode    = element->m_data.ignoreAlpha.has_value() ? (uint32_t)DISCARD_ALPHA : element->m_data.discardMode;

    const auto&       data   = element->m_data;
    float             alpha  = std::clamp(data.a, 0.f, 1.f);
    const auto        box    = element->m_data.box;
    CBox              newBox = box;
    m_renderer->m_renderData.renderModif.applyToBox(newBox);

    CBox transformedBox = newBox;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    const auto cb = g_pHyprVulkan->renderCB();
    cb->useTexture(element->m_data.tex);
    const auto texture = VKTEX(element->m_data.tex);

    // get the needed transform for this texture
    const auto MONITOR_INVERTED = Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform));
    auto       TRANSFORM        = texture->m_transform;

    if (m_renderer->monitorTransformEnabled())
        TRANSFORM = Math::composeTransform(MONITOR_INVERTED, TRANSFORM);

    const auto mat = m_renderer->projectBoxToTarget(element->m_data.box, TRANSFORM).getMatrix();

    // const auto&       vertData = matToVertShader(mat);
    SVkVertShaderData vertData = {
        .mat4 =
            {
                {mat[0], mat[1], 0, mat[2]},
                {mat[3], mat[4], 0, mat[5]},
                {0, 0, 1, 0},
                {0, 0, 0, 1},
            },
        .uvOffset = {element->m_data.box.x / m_renderData.pMonitor->m_transformedSize.x, element->m_data.box.y / m_renderData.pMonitor->m_transformedSize.y},
        .uvSize   = {element->m_data.box.width / m_renderData.pMonitor->m_transformedSize.x, element->m_data.box.height / m_renderData.pMonitor->m_transformedSize.y},
    };

    SVkFragShaderData fragData = {
        .alpha = alpha,
    };
    uint8_t usedFeatures = 0;

    switch (texture->m_type) {
        case TEXTURE_RGBA: usedFeatures |= SH_FEAT_RGBA; break;
        case TEXTURE_RGBX: usedFeatures &= ~SH_FEAT_RGBA; break;

        // TODO set correct features
        case TEXTURE_EXTERNAL: break; // might be unused
        default: RASSERT(false, "tex->m_iTarget unsupported!");
    }
    if (m_renderData.currentWindow && m_renderData.currentWindow->m_ruleApplicator->RGBX().valueOrDefault())
        usedFeatures &= ~SH_FEAT_RGBA;

    if (element->m_data.blur)
        usedFeatures |= SH_FEAT_BLUR;

    // if (element->m_data.blur) {
    //     auto el = makeUnique<CTexPassElement>(CTexPassElement::SRenderData{
    //         .tex = element->m_data.blurredBG,
    //         .box = CBox{{0, 0}, m_renderData.pMonitor->m_transformedSize},
    //         // .clipRegion = data.clipRegion,
    //         .clipRegion = element->m_data.box,
    //     });
    //     draw(el.get(), damage);
    // }

    if (element->m_data.discardActive || (element->m_data.blur && discardMode != 0 && (!data.blockBlurOptimization || (discardMode & DISCARD_ALPHA)))) {
        usedFeatures |= SH_FEAT_DISCARD;
        fragData.discardMode       = discardMode;
        fragData.discardAlphaValue = discardOpacity;
    }

    if (m_renderData.currentWindow && (m_renderData.currentWindow->m_notRespondingTint->value() > 0 || m_renderData.currentWindow->m_dimPercent->value() > 0)) {
        usedFeatures |= SH_FEAT_TINT;
        fragData.tint = 1.0f -
            (m_renderer->m_renderData.currentWindow->m_notRespondingTint->value() > 0 ? m_renderer->m_renderData.currentWindow->m_notRespondingTint->value() :
                                                                                        m_renderer->m_renderData.currentWindow->m_dimPercent->value());
    }

    SRounding roundingData = {
        .radius   = element->m_data.round,
        .power    = data.roundingPower,
        .topLeft  = {sc<float>(TOPLEFT.x), sc<float>(TOPLEFT.y)},
        .fullSize = {sc<float>(FULLSIZE.x), sc<float>(FULLSIZE.y)},
    };
    if (roundingData.radius > 0)
        usedFeatures |= SH_FEAT_ROUNDING;

    const auto surface = m_renderer->m_renderData.surface;

    const auto imageDescription = surface.valid() && surface->m_colorManagement.valid() ?
        NColorManagement::CImageDescription::from(surface->m_colorManagement->imageDescription()) :
        (data.cmBackToSRGB ? data.cmBackToSRGBSource->m_imageDescription : NColorManagement::getDefaultImageDescription());

    auto       chosenSdrEotf          = sdrEOTF != NTransferFunction::TF_SRGB ? NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22 : NColorManagement::CM_TRANSFER_FUNCTION_SRGB;
    const auto targetImageDescription = data.cmBackToSRGB ? NColorManagement::CImageDescription::from(NColorManagement::SImageDescription{.transferFunction = chosenSdrEotf}) :
                                                            m_renderData.pMonitor->m_imageDescription;
    const bool skipCM                 = !*PCM                                              /* CM unsupported or disabled */
        || m_renderData.pMonitor->doesNoShaderCM()                                         /* no shader needed */
        || (imageDescription->id() == targetImageDescription->id() && !data.cmBackToSRGB); /* Source and target have the same image description */
    SShaderCM              cmData;
    SShaderTonemap         tonemapData;
    SShaderTargetPrimaries primariesData;
    if (!skipCM) {
        usedFeatures |= SH_FEAT_CM;
        auto settings = m_renderer->getCMSettings(imageDescription, targetImageDescription, m_renderData.surface.valid() ? m_renderData.surface.lock() : nullptr);
        usedFeatures |= passCMUniforms(settings, cmData, tonemapData, primariesData);
    }

    auto       pipeline = m_renderer->m_currentRenderPass->texturePipeline(usedFeatures);
    const auto layout   = pipeline->layout().lock();
    const auto view     = texture->getView(layout);
    if (!view)
        return;

    m_renderer->bindPipeline(pipeline);

    std::vector<VkDescriptorSet> ds = {view->vkDS()};

    if (element->m_data.blur) {
        const auto viewBlur = VKTEX(element->m_data.blurredBG)->getView(layout);
        if (viewBlur)
            ds.push_back(viewBlur->vkDS());
    }

    vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, ds.size(), ds.data(), 0, nullptr);

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

    // Log::logger->log(Log::DEBUG, "DRAW TEX {}x{}@{}x{}, damage {}x{}@{}x{}, clipbox {}x{}@{}x{}, clipregion {}x{}@{}x{}", element->m_data.box.width, element->m_data.box.height,
    //                  element->m_data.box.x, element->m_data.box.y,                                                                                                            //
    //                  m_renderData.damage.getExtents().width, m_renderData.damage.getExtents().height, m_renderData.damage.getExtents().x, m_renderData.damage.getExtents().y, //
    //                  m_renderData.clipBox.width, m_renderData.clipBox.height, m_renderData.clipBox.x, m_renderData.clipBox.y,                                                 //
    //                  element->m_data.clipRegion.getExtents().width, element->m_data.clipRegion.getExtents().height, element->m_data.clipRegion.getExtents().x,
    //                  element->m_data.clipRegion.getExtents().y //
    // );

    auto clipBox = !element->m_data.damage.empty() ? element->m_data.damage : (!damage.empty() ? damage : m_renderData.damage);
    if (!m_renderData.clipBox.empty() || !element->m_data.clipRegion.empty()) {
        CRegion damageClip = m_renderData.clipBox;

        if (!element->m_data.clipRegion.empty()) {
            if (m_renderData.clipBox.empty())
                damageClip = element->m_data.clipRegion;
            else
                damageClip.intersect(element->m_data.clipRegion);
        }
        clipBox = damageClip;
    }
    clipBox = clipBox.intersect(element->m_data.box);

    drawRegionRects(clipBox, cb->vk());

    texture->m_lastUsedCB = cb;

    if (element->m_data.tex->isDMA()) {
        m_renderer->setTexBarriers(element->m_data.tex);

        static bool warned = false;
        if (!warned) {
            warned = true;
            Log::logger->log(Log::WARN, "fixme: vulkan draw dma texture sync");
        }
    }
};

void CVKElementRenderer::draw(WP<CTextureMatteElement> element, const CRegion& damage) {
    VK_CB_LABEL_BEGIN(m_renderer->m_currentCommandBuffer->vk(), "Matte element")
    auto&            m_renderData        = m_renderer->m_renderData;
    static auto      PSHADOWIGNOREWINDOW = CConfigValue<Hyprlang::INT>("decoration:shadow:ignore_window");
    SP<IFramebuffer> mirrorFB;
    SP<IFramebuffer> mirrorSwapFB;
    if (*PSHADOWIGNOREWINDOW) {
        mirrorFB     = m_renderData.pMonitor->resources()->getUnusedWorkBuffer();
        mirrorSwapFB = m_renderData.pMonitor->resources()->getUnusedWorkBuffer();
        if (m_renderer->m_hasBoundFB)
            m_renderer->m_currentRenderPass->endRendering();
        m_renderer->m_currentCommandBuffer->changeLayout(
            mirrorFB, //
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
        m_renderer->m_currentCommandBuffer->changeLayout(
            mirrorSwapFB, //
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

        if (m_renderer->m_hasBoundFB)
            m_renderer->m_currentRenderPass->beginRendering(m_renderer->m_currentCommandBuffer.lock(), m_renderer->m_hasBoundFB);
    }

    const auto cb       = g_pHyprVulkan->renderCB();
    const auto tex      = element->m_data.tex;
    const auto texMatte = element->m_data.fb->getTexture();
    cb->useTexture(tex);
    cb->useTexture(texMatte);
    RASSERT((tex->ok()), "Attempted to draw nullptr texture!");

    TRACY_GPU_ZONE("RenderTextureMatte");

    CBox newBox = element->m_data.box;
    m_renderData.renderModif.applyToBox(newBox);

    const auto  mat = m_renderer->projectBoxToTarget(element->m_data.box).getMatrix();

    const auto& vertData = matToVertShader(mat);

    const auto  layout = m_renderer->m_currentRenderPass->textureMattePipeline()->layout().lock();

    const auto  view = VKTEX(tex)->getView(layout);
    if (!view) {
        VK_CB_LABEL_END(m_renderer->m_currentCommandBuffer->vk())
        return;
    }

    const auto viewMatte = VKTEX(texMatte)->getView(layout);
    if (!viewMatte) {
        VK_CB_LABEL_END(m_renderer->m_currentCommandBuffer->vk())
        return;
    }

    m_renderer->bindPipeline(m_renderer->m_currentRenderPass->textureMattePipeline());

    const std::array<VkDescriptorSet, 2> ds = {view->vkDS(), viewMatte->vkDS()};
    vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, ds.size(), ds.data(), 0, nullptr);

    vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
    drawRegionRects(m_renderData.damage, cb->vk());

    if (*PSHADOWIGNOREWINDOW) {
        if (m_renderer->m_hasBoundFB)
            m_renderer->m_currentRenderPass->endRendering();
        m_renderer->m_currentCommandBuffer->changeLayout(
            mirrorFB, //
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});
        m_renderer->m_currentCommandBuffer->changeLayout(
            mirrorSwapFB, //
            {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
            {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

        if (m_renderer->m_hasBoundFB)
            m_renderer->m_currentRenderPass->beginRendering(m_renderer->m_currentCommandBuffer.lock(), m_renderer->m_hasBoundFB);
    }
    VK_CB_LABEL_END(m_renderer->m_currentCommandBuffer->vk())
};