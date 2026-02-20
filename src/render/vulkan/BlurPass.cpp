#include "BlurPass.hpp"
#include "../Framebuffer.hpp"
#include "Framebuffer.hpp"
#include "VKTexture.hpp"
#include "Vulkan.hpp"
#include "render/Renderer.hpp"
#include "render/VKRenderer.hpp"
#include "render/vulkan/types.hpp"
#include "utils.hpp"
#include <hyprutils/math/Region.hpp>
#include <vulkan/vulkan_core.h>

CVKBlurPass::CVKBlurPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders, int passes) :
    IDeviceUser(device), m_drmFormat(format), m_shaders(shaders), m_passes(passes) {
    const auto info = m_device->getFormat(m_drmFormat);
    RASSERT(info.has_value(), "No info for drm format {}", NFormatUtils::drmFormatName(m_drmFormat));

    std::array<VkAttachmentDescription, 2> attachments = {VkAttachmentDescription{
                                                              .format         = info->format.vkFormat,
                                                              .samples        = VK_SAMPLE_COUNT_1_BIT,
                                                              .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
                                                              .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                                                              .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                              .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                              .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                          },
                                                          VkAttachmentDescription{
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
    VkAttachmentReference write1Ref = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference read1Ref = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentReference write2Ref = {
        .attachment = 1,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference read2Ref = {
        .attachment = 1,
        .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    std::vector<VkSubpassDescription> subpasses;
    subpasses.reserve(1 + (passes * 2) + 1);

    // Prepare
    subpasses.push_back({
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &write1Ref,
    });

    for (int i = 0; i < passes; i++) {
        // alternate buffers
        subpasses.push_back({
            .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 1,
            .pInputAttachments    = &read1Ref,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &write2Ref,
        });
        subpasses.push_back({
            .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 1,
            .pInputAttachments    = &read2Ref,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &write1Ref,
        });
    }

    // Finish
    subpasses.push_back({
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 1,
        .pInputAttachments    = &read1Ref,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &write2Ref,
    });

    std::vector<VkSubpassDependency> deps;
    deps.reserve(2 + subpasses.size() - 1);

    // input
    deps.push_back({
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
        .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
    });

    // between passes
    for (unsigned i = 0; i < subpasses.size() - 1; i++) {
        deps.push_back({
            .srcSubpass      = i,
            .dstSubpass      = i + 1,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });
    }

    // output
    deps.push_back({
        .srcSubpass    = subpasses.size() - 1,
        .dstSubpass    = VK_SUBPASS_EXTERNAL,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,
    });

    VkRenderPassCreateInfo rpInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .attachmentCount = attachments.size(),
        .pAttachments    = attachments.data(),
        .subpassCount    = subpasses.size(),
        .pSubpasses      = subpasses.data(),
        .dependencyCount = deps.size(),
        .pDependencies   = deps.data(),
    };

    IF_VKFAIL(vkCreateRenderPass, vkDevice(), &rpInfo, nullptr, &m_vkRenderPass) {
        LOG_VKFAIL;
    }

    m_pipelines.reserve(subpasses.size());
    m_pipelines.push_back(makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_prepare, CVkPipeline::SSettings{.subpass = 0, .blend = false}));
    for (int i = 0; i < passes; i++) {
        m_pipelines.push_back(makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_blur1, CVkPipeline::SSettings{.subpass = i + 1, .blend = false}));
    }
    for (int i = 0; i < passes; i++) {
        m_pipelines.push_back(
            makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_blur2, CVkPipeline::SSettings{.subpass = passes + i + 1, .blend = false}));
    }
    m_pipelines.push_back(
        makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_finish, CVkPipeline::SSettings{.subpass = subpasses.size() - 1, .blend = false}));
}

CVKBlurPass::~CVKBlurPass() {
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

SP<ITexture> CVKBlurPass::blurTexture(SP<ITexture> tex, SP<IFramebuffer> first, SP<IFramebuffer> second) {
    if (!m_vkRenderPass || !tex)
        return tex;

    static auto PBLURSIZE             = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURVIBRANCY         = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy");
    static auto PBLURVIBRANCYDARKNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy_darkness");
    static auto PBLURCONTRAST         = CConfigValue<Hyprlang::FLOAT>("decoration:blur:contrast");
    static auto PBLURBRIGHTNESS       = CConfigValue<Hyprlang::FLOAT>("decoration:blur:brightness");
    static auto PBLURNOISE            = CConfigValue<Hyprlang::FLOAT>("decoration:blur:noise");

    const auto  texture  = dc<CVKTexture*>(tex.get());
    const auto  renderer = dc<CHyprVKRenderer*>(g_pHyprRenderer.get());
    const auto  cb       = g_pHyprVulkan->renderCB();
    cb->useTexture(tex);
    cb->useTexture(first->getTexture());
    cb->useTexture(second->getTexture());

    cb->changeLayout(dc<CVKTexture*>(first->getTexture().get())->m_image, //
                     {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                     {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    cb->changeLayout(dc<CVKTexture*>(second->getTexture().get())->m_image, //
                     {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                     {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT, .accessMask = 0});

    const auto  mat = g_pHyprRenderer->projectBoxToTarget({{0, 0}, texture->m_size}, texture->m_transform).getMatrix();

    const auto& vertData = matToVertShader(mat);

    VkImageView attachments[] = {
        dc<CVKTexture*>(first->getTexture().get())->vkView(),
        dc<CVKTexture*>(second->getTexture().get())->vkView(),
    };

    VkFramebufferCreateInfo fbInfo = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .flags           = 0,
        .renderPass      = m_vkRenderPass,
        .attachmentCount = 2,
        .pAttachments    = attachments,
        .width           = texture->m_size.x,
        .height          = texture->m_size.y,
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
                .extent = {texture->m_size.x, texture->m_size.y},
            },
        .clearValueCount = 0,
    };

    vkCmdBeginRenderPass(g_pHyprVulkan->renderCB()->vk(), &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {
        .width    = texture->m_size.x,
        .height   = texture->m_size.y,
        .maxDepth = 1,
    };
    vkCmdSetViewport(g_pHyprVulkan->renderCB()->vk(), 0, 1, &viewport);

    // prepare
    {
        const auto layout = m_pipelines[0]->layout().lock();
        const auto view   = texture->getView(layout);
        if (!view)
            return tex;

        SVkPrepareShaderData fragData = {
            .contrast   = *PBLURCONTRAST,
            .brightness = *PBLURBRIGHTNESS,
        };

        renderer->bindPipeline(m_pipelines[0]);

        const auto ds = view->vkDS();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

        CRegion clipBox = {0, 0, texture->m_size.x, texture->m_size.y};

        drawRegionRects(clipBox, cb->vk());
    }

    auto current = first;

    for (int i = 0; i < m_passes; i++) {
        // blur1

        const auto pipeline = m_pipelines[i + 1];
        vkCmdNextSubpass(g_pHyprVulkan->renderCB()->vk(), VK_SUBPASS_CONTENTS_INLINE);
        const auto layout = pipeline->layout().lock();
        const auto view   = dc<CVKTexture*>(current->getTexture().get())->getView(layout);
        if (!view)
            return tex;

        SVkBlur1ShaderData fragData = {
            .radius = *PBLURSIZE, // TODO * a
            // .halfpixel        = {0.5f / (g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.x / 2.f), 0.5f / (g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.y / 2.f)},
            .halfpixel        = {0.5f / (texture->m_size.x / 2.f), 0.5f / (texture->m_size.y / 2.f)},
            .passes           = m_passes,
            .vibrancy         = *PBLURVIBRANCY,
            .vibrancyDarkness = *PBLURVIBRANCYDARKNESS,
        };

        renderer->bindPipeline(pipeline);

        const auto ds = view->vkDS();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

        CRegion clipBox = {0, 0, texture->m_size.x, texture->m_size.y};

        drawRegionRects(clipBox, cb->vk());

        current = current == first ? second : first;
    }

    for (int i = 0; i < m_passes; i++) {
        // blur2

        const auto pipeline = m_pipelines[m_passes + i + 1];
        vkCmdNextSubpass(g_pHyprVulkan->renderCB()->vk(), VK_SUBPASS_CONTENTS_INLINE);
        const auto layout = pipeline->layout().lock();
        const auto view   = dc<CVKTexture*>(current->getTexture().get())->getView(layout);
        if (!view)
            return tex;

        SVkBlur2ShaderData fragData = {
            .radius = *PBLURSIZE, // TODO * a
            // .halfpixel = {0.5f / (g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.x * 2.f), 0.5f / (g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.y * 2.f)},
            .halfpixel = {0.5f / (texture->m_size.x * 2.f), 0.5f / (texture->m_size.y * 2.f)},
        };

        renderer->bindPipeline(pipeline);

        const auto ds = view->vkDS();
        vkCmdBindDescriptorSets(cb->vk(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk(), 0, 1, &ds, 0, nullptr);

        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertData), &vertData);
        vkCmdPushConstants(cb->vk(), layout->vk(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vertData), sizeof(fragData), &fragData);

        CRegion clipBox = {0, 0, texture->m_size.x, texture->m_size.y};

        drawRegionRects(clipBox, cb->vk());

        current = current == first ? second : first;
    }

    // finish
    {
        vkCmdNextSubpass(g_pHyprVulkan->renderCB()->vk(), VK_SUBPASS_CONTENTS_INLINE);

        const auto layout = m_pipelines.back()->layout().lock();
        const auto view   = dc<CVKTexture*>(first->getTexture().get())->getView(layout);
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

        CRegion clipBox = {0, 0, texture->m_size.x, texture->m_size.y};

        drawRegionRects(clipBox, cb->vk());
    }

    vkCmdEndRenderPass(g_pHyprVulkan->renderCB()->vk());

    return second->getTexture();
}
