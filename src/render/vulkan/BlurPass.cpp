#include "BlurPass.hpp"
#include "../Framebuffer.hpp"
#include "Framebuffer.hpp"
#include "VKTexture.hpp"
#include "Vulkan.hpp"
#include "helpers/cm/ColorManagement.hpp"
#include "render/Renderer.hpp"
#include "render/ShaderLoader.hpp"
#include "render/Texture.hpp"
#include "render/VKRenderer.hpp"
#include "render/vulkan/CommandBuffer.hpp"
#include "render/vulkan/types.hpp"
#include "utils.hpp"
#include <hyprutils/math/Misc.hpp>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <vulkan/vulkan_core.h>

using namespace Render::VK;

CVKBlurPass::CVKBlurPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders, int passes) :
    IDeviceUser(device), m_drmFormat(format), m_shaders(shaders), m_passes(passes) {
    const auto info = m_device->getFormat(m_drmFormat);
    RASSERT(info.has_value(), "No info for drm format {}", NFormatUtils::drmFormatName(m_drmFormat));

    m_preparePipeline   = makeShared<CVkPipeline>(m_device, info->format.vkFormat, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLURPREPARE),
                                                  CVkPipeline::SSettings{.subpass = 0, .blend = false});
    m_prepareCMPipeline = makeShared<CVkPipeline>(m_device, info->format.vkFormat, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLURPREPARE, SH_FEAT_CM),
                                                  CVkPipeline::SSettings{.subpass = 0, .blend = false});
    m_blur1Pipeline     = makeShared<CVkPipeline>(m_device, info->format.vkFormat, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLUR1),
                                                  CVkPipeline::SSettings{.subpass = 0, .blend = false});
    m_blur2Pipeline     = makeShared<CVkPipeline>(m_device, info->format.vkFormat, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLUR2),
                                                  CVkPipeline::SSettings{.subpass = 0, .blend = false});
    m_finishPipeline    = makeShared<CVkPipeline>(m_device, info->format.vkFormat, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLURFINISH),
                                                  CVkPipeline::SSettings{.subpass = 0, .blend = false});
}

CVKBlurPass::~CVKBlurPass() = default;

DRMFormat CVKBlurPass::format() {
    return m_drmFormat;
}

int CVKBlurPass::passes() {
    return m_passes;
}

static void beginRendering(WP<CHyprVkCommandBuffer> cb, WP<CVKFramebuffer> target) {
    cb->changeLayout(target, //
                     {.layout = target->m_lastKnownLayout, .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                     {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, .accessMask = 0});

    VkRenderingAttachmentInfo attachment = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = VKTEX(target->getTexture())->vkView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
    };

    VkRenderingInfo info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            {
                .offset = {0, 0},
                .extent = {target->m_size.x, target->m_size.y},
            },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &attachment,
    };

    vkCmdBeginRendering(cb->vk(), &info);

    VkViewport viewport = {
        .width    = target->m_size.x,
        .height   = target->m_size.y,
        .maxDepth = 1,
    };
    vkCmdSetViewport(g_pHyprVulkan->renderCB()->vk(), 0, 1, &viewport);
}

static void endRendering(WP<CHyprVkCommandBuffer> cb, WP<CVKFramebuffer> target) {
    vkCmdEndRendering(cb->vk());

    cb->changeLayout(target, //
                     {.layout = target->m_lastKnownLayout, .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                     {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, .accessMask = 0});
}

SP<Render::ITexture> CVKBlurPass::blurTexture(SP<ITexture> tex, SP<IFramebuffer> first, SP<IFramebuffer> second, float a, const CRegion& damage) {
    if (!tex)
        return tex;

    static auto PBLURSIZE             = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURVIBRANCY         = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy");
    static auto PBLURVIBRANCYDARKNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy_darkness");
    static auto PBLURCONTRAST         = CConfigValue<Hyprlang::FLOAT>("decoration:blur:contrast");
    static auto PBLURBRIGHTNESS       = CConfigValue<Hyprlang::FLOAT>("decoration:blur:brightness");
    static auto PBLURNOISE            = CConfigValue<Hyprlang::FLOAT>("decoration:blur:noise");

    auto        fbSize  = first->getTexture()->m_size;
    CRegion     clipBox = {0, 0, fbSize.x, fbSize.y};
    // const CRegion& clipBox = damage;

    const auto texture  = VKTEX(tex);
    const auto renderer = dc<CHyprVKRenderer*>(g_pHyprRenderer.get());
    const auto cb       = g_pHyprVulkan->renderCB();
    cb->useTexture(tex);
    cb->useTexture(first->getTexture());
    cb->useTexture(second->getTexture());

    const auto  TRANSFORM  = Math::wlTransformToHyprutils(Math::invertTransform(g_pHyprRenderer->m_renderData.pMonitor->m_transform));
    CBox        MONITORBOX = {0, 0, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.x, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.y};

    const auto  mat = g_pHyprRenderer->projectBoxToTarget(MONITORBOX, TRANSFORM).getMatrix();

    const auto& vertData = matToVertShader(mat);

    // prepare
    {
        VK_CB_LABEL_BEGIN(cb->vk(), "Prepare blur");
        beginRendering(cb, dynamicPointerCast<CVKFramebuffer>(first));

        static const auto PCM      = CConfigValue<Hyprlang::INT>("render:cm_enabled");
        const bool        skipCM   = !*PCM || g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription->id() == NColorManagement::getDefaultImageDescription()->id();
        const auto        pipeline = skipCM ? m_preparePipeline : m_prepareCMPipeline;
        const auto        layout   = pipeline->layout().lock();
        const auto        view     = texture->getView(layout);
        if (!view)
            return tex;

        // FIXME weird transform
        const auto TRANSFORM =
            g_pHyprRenderer->m_renderData.pMonitor->m_transform == WL_OUTPUT_TRANSFORM_90 || g_pHyprRenderer->m_renderData.pMonitor->m_transform == WL_OUTPUT_TRANSFORM_270 ?
            HYPRUTILS_TRANSFORM_180 :
            HYPRUTILS_TRANSFORM_NORMAL;
        const auto           mat = g_pHyprRenderer->projectBoxToTarget(MONITORBOX, TRANSFORM).getMatrix();

        const auto&          vertData = matToVertShader(mat);

        SVkPrepareShaderData fragData = {
            .contrast                = *PBLURCONTRAST,
            .brightness              = *PBLURBRIGHTNESS,
            .sdrBrightnessMultiplier = 1.0f,
        };

        SShaderCM cmData;
        if (!skipCM) {
            auto        settings = g_pHyprRenderer->getCMSettings(g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription, NColorManagement::getDefaultImageDescription(),
                                                                  g_pHyprRenderer->m_renderData.surface.valid() ? g_pHyprRenderer->m_renderData.surface.lock() : nullptr);
            const auto& mat      = settings.convertMatrix;

            cmData = {
                .sourceTF        = settings.sourceTF,
                .targetTF        = settings.targetTF,
                .srcRefLuminance = settings.srcRefLuminance,

                .srcTFRange = {settings.srcTFRange.min, settings.srcTFRange.max},
                .dstTFRange = {settings.dstTFRange.min, settings.dstTFRange.max},

                .convertMatrix =
                    {
                        {mat[0][0], mat[0][1], mat[0][2]}, //
                        {mat[1][0], mat[1][1], mat[1][2]}, //
                        {mat[2][0], mat[2][1], mat[2][2]}, //,
                    },
            };
            fragData.sdrBrightnessMultiplier = settings.sdrBrightnessMultiplier;
        }

        renderer->bindPipeline(pipeline);

        const auto ds = view->vkDS();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);
        if (!skipCM)
            vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData) + sizeof(fragData), sizeof(cmData), &cmData);

        drawRegionRects(clipBox, cb->vk(), false);

        endRendering(cb, dynamicPointerCast<CVKFramebuffer>(first));
        VK_CB_LABEL_END(cb->vk());
    }

    const SP<IFramebuffer> buffers[2] = {first, second};
    int                    sourceIdx  = 0;

    for (int i = 0; i < m_passes; i++) {
        // blur1
        int targetIdx = (sourceIdx + 1) % 2;
        VK_CB_LABEL_BEGIN(cb->vk(), std::format("Blur 1 pass {}", i));
        beginRendering(cb, dynamicPointerCast<CVKFramebuffer>(buffers[targetIdx]));

        const auto pipeline = m_blur1Pipeline;
        const auto layout   = pipeline->layout().lock();
        const auto view     = VKTEX(buffers[sourceIdx]->getTexture())->getView(layout);
        if (!view)
            return tex;

        SVkBlur1ShaderData fragData = {
            .radius           = *PBLURSIZE * a,
            .halfpixel        = {0.5f / (g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.x / 2.f), 0.5f / (g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.y / 2.f)},
            .passes           = m_passes,
            .vibrancy         = *PBLURVIBRANCY,
            .vibrancyDarkness = *PBLURVIBRANCYDARKNESS,
        };

        renderer->bindPipeline(pipeline);

        const auto ds = view->vkDS();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

        drawRegionRects(clipBox, cb->vk(), false);

        endRendering(cb, dynamicPointerCast<CVKFramebuffer>(buffers[targetIdx]));
        VK_CB_LABEL_END(cb->vk());

        sourceIdx = targetIdx;
    }

    for (int i = 0; i < m_passes; i++) {
        // blur2
        int targetIdx = (sourceIdx + 1) % 2;
        VK_CB_LABEL_BEGIN(cb->vk(), std::format("Blur 2 pass {}", i));
        beginRendering(cb, dynamicPointerCast<CVKFramebuffer>(buffers[targetIdx]));

        const auto pipeline = m_blur2Pipeline;
        const auto layout   = pipeline->layout().lock();
        const auto view     = VKTEX(buffers[sourceIdx]->getTexture())->getView(layout);
        if (!view)
            return tex;

        SVkBlur2ShaderData fragData = {
            .radius    = *PBLURSIZE * a,
            .halfpixel = {0.5f / (g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.x * 2.f), 0.5f / (g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.y * 2.f)},
        };

        renderer->bindPipeline(pipeline);

        const auto ds = view->vkDS();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

        drawRegionRects(clipBox, cb->vk(), false);

        endRendering(cb, dynamicPointerCast<CVKFramebuffer>(buffers[targetIdx]));
        VK_CB_LABEL_END(cb->vk());

        sourceIdx = targetIdx;
    }

    // finish
    {
        VK_CB_LABEL_BEGIN(cb->vk(), "Finish blur");
        beginRendering(cb, dynamicPointerCast<CVKFramebuffer>(second));

        const auto layout = m_finishPipeline->layout().lock();
        const auto view   = VKTEX(first->getTexture())->getView(layout);
        if (!view)
            return tex;

        SVkFinishShaderData fragData = {
            .noise      = *PBLURNOISE,
            .brightness = *PBLURBRIGHTNESS,
        };

        renderer->bindPipeline(m_finishPipeline);

        const auto ds = view->vkDS();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

        drawRegionRects(clipBox, cb->vk(), false);

        endRendering(cb, dynamicPointerCast<CVKFramebuffer>(second));
        VK_CB_LABEL_END(cb->vk());
    }

    VKFB(first)->m_lastKnownLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VKFB(second)->m_lastKnownLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    second->getTexture()->m_transform = TRANSFORM;
    return second->getTexture();
}
