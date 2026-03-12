#include "BlurPass.hpp"
#include "../Framebuffer.hpp"
#include "Framebuffer.hpp"
#include "VKTexture.hpp"
#include "Vulkan.hpp"
#include "render/Renderer.hpp"
#include "render/ShaderLoader.hpp"
#include "render/VKRenderer.hpp"
#include "render/vulkan/types.hpp"
#include "utils.hpp"
#include <hyprutils/math/Misc.hpp>
#include <hyprutils/math/Region.hpp>
#include <vulkan/vulkan_core.h>

using namespace Render::VK;

CVKBlurPass::CVKBlurPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders, int passes) :
    IDeviceUser(device), m_drmFormat(format), m_shaders(shaders), m_passes(passes) {
    const auto info = m_device->getFormat(m_drmFormat);
    RASSERT(info.has_value(), "No info for drm format {}", NFormatUtils::drmFormatName(m_drmFormat));

    std::array<VkAttachmentDescription2, 2> attachments = {VkAttachmentDescription2{
                                                               .sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                                                               .format         = info->format.vkFormat,
                                                               .samples        = VK_SAMPLE_COUNT_1_BIT,
                                                               .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
                                                               .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                                                               .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                               .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                               .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                               .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                           },
                                                           VkAttachmentDescription2{
                                                               .sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                                                               .format         = info->format.vkFormat,
                                                               .samples        = VK_SAMPLE_COUNT_1_BIT,
                                                               .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
                                                               .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                                                               .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                               .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                               .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                               .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                           }};
    //
    VkAttachmentReference2 write1Ref = {
        .sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    VkAttachmentReference2 read1Ref = {
        .sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    VkAttachmentReference2 write2Ref = {
        .sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .attachment = 1,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    VkAttachmentReference2 read2Ref = {
        .sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .attachment = 1,
        .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    std::vector<VkSubpassDescription2> subpasses;
    subpasses.reserve(1 + (passes * 2) + 1);

    // Prepare
    subpasses.push_back({
        .sType                = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &write1Ref,
    });

    for (int i = 0; i < passes; i++) {
        // alternate buffers
        subpasses.push_back({
            .sType                = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
            .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 1,
            .pInputAttachments    = &read1Ref,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &write2Ref,
        });
        subpasses.push_back({
            .sType                = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
            .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 1,
            .pInputAttachments    = &read2Ref,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &write1Ref,
        });
    }

    // Finish
    subpasses.push_back({
        .sType                = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 1,
        .pInputAttachments    = &read1Ref,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &write2Ref,
    });

    std::vector<VkSubpassDependency2> deps;
    deps.reserve(2 + subpasses.size() - 1);

    // input
    deps.push_back({
        .sType         = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
        .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    });

    // between passes
    for (unsigned i = 0; i < subpasses.size() - 1; i++) {
        deps.push_back({
            .sType         = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
            .srcSubpass    = i,
            .dstSubpass    = i + 1,
            .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            // .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });
    }

    // output
    deps.push_back({
        .sType         = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
        .srcSubpass    = subpasses.size() - 1,
        .dstSubpass    = VK_SUBPASS_EXTERNAL,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,
    });

    VkRenderPassCreateInfo2 rpInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
        .pNext           = nullptr,
        .flags           = 0,
        .attachmentCount = attachments.size(),
        .pAttachments    = attachments.data(),
        .subpassCount    = subpasses.size(),
        .pSubpasses      = subpasses.data(),
        .dependencyCount = deps.size(),
        .pDependencies   = deps.data(),
    };

    IF_VKFAIL(vkCreateRenderPass2, vkDevice(), &rpInfo, nullptr, &m_vkRenderPass) {
        LOG_VKFAIL;
        return;
    }
    SET_VK_PASS_NAME(m_vkRenderPass, "Blur RP")

    m_pipelines.reserve(subpasses.size() - 1);

    m_preparePipeline   = makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLURPREPARE),
                                                  CVkPipeline::SSettings{.subpass = 0, .blend = false});
    m_prepareCMPipeline = makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLURPREPARE, SH_FEAT_CM),
                                                  CVkPipeline::SSettings{.subpass = 0, .blend = false});
    for (int i = 0; i < passes; i++) {
        m_pipelines.push_back(makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLUR1),
                                                      CVkPipeline::SSettings{.subpass = i + 1, .blend = false}));
    }
    for (int i = 0; i < passes; i++) {
        m_pipelines.push_back(makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLUR2),
                                                      CVkPipeline::SSettings{.subpass = passes + i + 1, .blend = false}));
    }
    m_pipelines.push_back(makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->getShaderVariant(SH_FRAG_BLURFINISH),
                                                  CVkPipeline::SSettings{.subpass = subpasses.size() - 1, .blend = false}));
}

CVKBlurPass::~CVKBlurPass() {
    m_preparePipeline.reset();
    m_prepareCMPipeline.reset();
    m_pipelines.clear();

    if (m_vkRenderPass)
        vkDestroyRenderPass(vkDevice(), m_vkRenderPass, nullptr);
}

DRMFormat CVKBlurPass::format() {
    return m_drmFormat;
}

VkRenderPass CVKBlurPass::vk() {
    return m_vkRenderPass;
}

int CVKBlurPass::passes() {
    return m_passes;
}

SP<Render::ITexture> CVKBlurPass::blurTexture(SP<ITexture> tex, SP<IFramebuffer> first, SP<IFramebuffer> second, float a, const CRegion& damage) {
    if (!m_vkRenderPass || !tex)
        return tex;

    auto    fbSize  = first->getTexture()->m_size;
    CRegion clipBox = {0, 0, fbSize.x, fbSize.y};
    // const CRegion& clipBox = damage;

    static auto PBLURSIZE             = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURVIBRANCY         = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy");
    static auto PBLURVIBRANCYDARKNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy_darkness");
    static auto PBLURCONTRAST         = CConfigValue<Hyprlang::FLOAT>("decoration:blur:contrast");
    static auto PBLURBRIGHTNESS       = CConfigValue<Hyprlang::FLOAT>("decoration:blur:brightness");
    static auto PBLURNOISE            = CConfigValue<Hyprlang::FLOAT>("decoration:blur:noise");

    const auto  texture  = VKTEX(tex);
    const auto  renderer = dc<CHyprVKRenderer*>(g_pHyprRenderer.get());
    const auto  cb       = g_pHyprVulkan->renderCB();
    cb->useTexture(tex);
    cb->useTexture(first->getTexture());
    cb->useTexture(second->getTexture());

    cb->changeLayout(VKTEX(first->getTexture())->m_image, //
                     {.layout = VKFB(first)->m_lastKnownLayout, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                     {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    cb->changeLayout(VKTEX(second->getTexture())->m_image, //
                     {.layout = VKFB(second)->m_lastKnownLayout, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                     {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    const auto  TRANSFORM  = Math::wlTransformToHyprutils(Math::invertTransform(g_pHyprRenderer->m_renderData.pMonitor->m_transform));
    CBox        MONITORBOX = {0, 0, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.x, g_pHyprRenderer->m_renderData.pMonitor->m_transformedSize.y};

    const auto  mat = g_pHyprRenderer->projectBoxToTarget(MONITORBOX, TRANSFORM).getMatrix();

    const auto& vertData = matToVertShader(mat);

    VkImageView attachments[] = {
        VKTEX(first->getTexture())->vkView(),
        VKTEX(second->getTexture())->vkView(),
    };

    VkFramebufferCreateInfo fbInfo = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .flags           = 0,
        .renderPass      = m_vkRenderPass,
        .attachmentCount = 2,
        .pAttachments    = attachments,
        .width           = fbSize.x,
        .height          = fbSize.y,
        .layers          = 1,
    };

    VkFramebuffer fb;
    IF_VKFAIL(vkCreateFramebuffer, vkDevice(), &fbInfo, nullptr, &fb) {
        LOG_VKFAIL;
        return tex;
    }

    VkRenderPassBeginInfo info = {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = m_vkRenderPass,
        .framebuffer = fb,
        .renderArea =
            {
                .extent = {fbSize.x, fbSize.y},
            },
        .clearValueCount = 0,
    };

    VkSubpassBeginInfo subInfo = {
        .sType    = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
        .contents = VK_SUBPASS_CONTENTS_INLINE,
    };
    VkSubpassEndInfo subEndInfo = {.sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO};

    vkCmdBeginRenderPass2(g_pHyprVulkan->renderCB()->vk(), &info, &subInfo);

    VkViewport viewport = {
        .width    = fbSize.x,
        .height   = fbSize.y,
        .maxDepth = 1,
    };
    vkCmdSetViewport(g_pHyprVulkan->renderCB()->vk(), 0, 1, &viewport);

    // prepare
    {
        static const auto PCM      = CConfigValue<Hyprlang::INT>("render:cm_enabled");
        const bool        skipCM   = !*PCM || g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription->id() == NColorManagement::DEFAULT_IMAGE_DESCRIPTION->id();
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
            auto        settings = g_pHyprRenderer->getCMSettings(g_pHyprRenderer->m_renderData.pMonitor->m_imageDescription, NColorManagement::DEFAULT_IMAGE_DESCRIPTION,
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
    }

    auto current = first;

    for (int i = 0; i < m_passes; i++) {
        // blur1
        const auto pipeline = m_pipelines[i];
        vkCmdNextSubpass2(g_pHyprVulkan->renderCB()->vk(), &subInfo, &subEndInfo);
        const auto layout = pipeline->layout().lock();
        const auto view   = VKTEX(current->getTexture())->getView(layout);
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

        current = current == first ? second : first;
    }

    for (int i = 0; i < m_passes; i++) {
        // blur2

        const auto pipeline = m_pipelines[m_passes + i];
        vkCmdNextSubpass2(g_pHyprVulkan->renderCB()->vk(), &subInfo, &subEndInfo);
        const auto layout = pipeline->layout().lock();
        const auto view   = VKTEX(current->getTexture())->getView(layout);
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

        current = current == first ? second : first;
    }

    // finish
    {
        vkCmdNextSubpass2(g_pHyprVulkan->renderCB()->vk(), &subInfo, &subEndInfo);

        const auto layout = m_pipelines.back()->layout().lock();
        const auto view   = VKTEX(first->getTexture())->getView(layout);
        if (!view)
            return tex;

        SVkFinishShaderData fragData = {
            .noise      = *PBLURNOISE,
            .brightness = *PBLURBRIGHTNESS,
        };

        renderer->bindPipeline(m_pipelines.back());

        const auto ds = view->vkDS();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

        drawRegionRects(clipBox, cb->vk(), false);
    }

    g_pHyprVulkan->renderCB()->endRenderPass();

    VKFB(first)->m_lastKnownLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VKFB(second)->m_lastKnownLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    second->getTexture()->m_transform = TRANSFORM;
    return second->getTexture();
}
