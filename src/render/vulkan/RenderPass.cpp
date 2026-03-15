#include "RenderPass.hpp"
#include "DeviceUser.hpp"
#include "../../helpers/Format.hpp"
#include "macros.hpp"
#include "../ShaderLoader.hpp"
#include "Vulkan.hpp"
#include "utils.hpp"
#include "types.hpp"
#include <hyprutils/memory/Casts.hpp>

using namespace Render::VK;

CVkRenderPass::CVkRenderPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders) : IDeviceUser(device), m_drmFormat(format), m_shaders(shaders) {
    const auto info = m_device->getFormat(m_drmFormat);
    RASSERT(info.has_value(), "No info for drm format {}", NFormatUtils::drmFormatName(m_drmFormat));
    m_vkFormat = info->format.vkFormat;

    // VkAttachmentDescription2 attachment = {
    //     .sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
    //     .format         = info->format.vkFormat,
    //     .samples        = VK_SAMPLE_COUNT_1_BIT,
    //     .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
    //     .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
    //     .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    //     .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    //     .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
    //     .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
    // };

    // VkAttachmentReference2 colorRef = {
    //     .sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
    //     .attachment = 0,
    //     .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    // };

    // VkSubpassDescription2 subpass = {
    //     .sType                = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
    //     .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
    //     .colorAttachmentCount = 1,
    //     .pColorAttachments    = &colorRef,
    // };

    // std::vector<VkSubpassDependency2> deps = {
    //     {
    //         .sType         = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
    //         .srcSubpass    = VK_SUBPASS_EXTERNAL,
    //         .dstSubpass    = 0,
    //         .srcStageMask  = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    //         .dstStageMask  = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    //         .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    //         .dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
    //             VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    //     },
    //     {
    //         .sType         = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
    //         .srcSubpass    = 0,
    //         .dstSubpass    = VK_SUBPASS_EXTERNAL,
    //         .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    //         .dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    //         .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    //         .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,
    //     },
    // };

    // VkRenderPassCreateInfo2 rpInfo = {
    //     .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
    //     .attachmentCount = 1,
    //     .pAttachments    = &attachment,
    //     .subpassCount    = 1,
    //     .pSubpasses      = &subpass,
    //     .dependencyCount = deps.size(),
    //     .pDependencies   = deps.data(),
    // };
}

WP<CVkPipeline> CVkRenderPass::borderPipeline(uint8_t features) {
    return getPipeline(SH_FRAG_BORDER1, features, 0, 1);
}

WP<CVkPipeline> CVkRenderPass::rectPipeline(uint8_t features) {
    return getPipeline(SH_FRAG_QUAD, features);
}

WP<CVkPipeline> CVkRenderPass::shadowPipeline(uint8_t features) {
    return getPipeline(SH_FRAG_SHADOW, features);
}

WP<CVkPipeline> CVkRenderPass::texturePipeline(uint8_t features) {
    return getPipeline(SH_FRAG_SURFACE, features, 2);
}

WP<CVkPipeline> CVkRenderPass::textureMattePipeline(uint8_t features) {
    return getPipeline(SH_FRAG_MATTE, features, 2);
}

WP<CVkPipeline> CVkRenderPass::passPipeline(uint8_t features) {
    return getPipeline(SH_FRAG_PASSTHRURGBA, features);
}

SP<CVkPipeline> CVkRenderPass::getPipeline(ePreparedFragmentShader frag, uint8_t features, int texCount, int uboCount) {
    if (!m_pipelines[frag].contains(features))
        m_pipelines[frag][features] = makeShared<CVkPipeline>(m_device, m_vkFormat, m_shaders->m_vert, m_shaders->getShaderVariant(frag, features),
                                                              CVkPipeline::SSettings{.texCount = texCount, .uboCount = uboCount});

    return m_pipelines[frag][features];
}

void CVkRenderPass::beginRendering(SP<CHyprVkCommandBuffer> cb, SP<CVKFramebuffer> target) {
    ASSERT(!m_targetFB)
    m_cmdBuffer = cb;
    m_targetFB  = target;
    VK_CB_LABEL_BEGIN(cb->vk(), "CVkRenderPass::beginRendering");
    cb->changeLayout(VKTEX(target->getTexture())->m_image, //
                     {
                         .layout     = target->m_lastKnownLayout,
                         .stageMask  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                         .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                     },
                     {
                         .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         .stageMask  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                         .accessMask = 0,
                     });

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

void CVkRenderPass::endRendering() {
    vkCmdEndRendering(m_cmdBuffer->vk());

    m_cmdBuffer->changeLayout(VKTEX(m_targetFB->getTexture())->m_image, //
                              {.layout = m_targetFB->m_lastKnownLayout, .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                              {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, .accessMask = 0});

    VK_CB_LABEL_END(m_cmdBuffer->vk())
    m_targetFB.reset();
    m_cmdBuffer.reset();
}

CVkRenderPass::~CVkRenderPass() = default;