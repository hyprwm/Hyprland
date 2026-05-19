#include "RenderPass.hpp"
#include "DeviceUser.hpp"
#include "../../helpers/Format.hpp"
#include "macros.hpp"
#include "../ShaderLoader.hpp"
#include "Vulkan.hpp"
#include "utils.hpp"
#include "types.hpp"
#include <hyprutils/memory/Casts.hpp>
#include <vulkan/vulkan_core.h>

using namespace Render::VK;

CVkRenderPass::CVkRenderPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders) : IDeviceUser(device), m_drmFormat(format), m_shaders(shaders) {
    const auto info = m_device->getFormat(m_drmFormat);
    RASSERT(info.has_value(), "No info for drm format {}", NFormatUtils::drmFormatName(m_drmFormat));
    m_vkFormat = info->format.vkFormat;
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
    VK_CB_LABEL_BEGIN(cb->vk(), "render pass start");
    cb->changeLayout(target, //
                     {
                         .layout     = target->m_lastKnownLayout,
                         .stageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                     },
                     {
                         .layout     = VK_IMAGE_LAYOUT_GENERAL,
                         .stageMask  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                         .accessMask = 0,
                     });

    VkRenderingAttachmentInfo attachment = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = VKTEX(target->getTexture())->vkView(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
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
    VK_CB_LABEL_END(m_cmdBuffer->vk())
}

void CVkRenderPass::endRendering() {
    VK_CB_LABEL_BEGIN(m_cmdBuffer->vk(), "render pass end");
    vkCmdEndRendering(m_cmdBuffer->vk());

    m_cmdBuffer->changeLayout(m_targetFB, //
                              {.layout = m_targetFB->m_lastKnownLayout, .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT},
                              {.layout = VK_IMAGE_LAYOUT_GENERAL, .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, .accessMask = 0});
    VK_CB_LABEL_END(m_cmdBuffer->vk())
    VK_CB_LABEL_END(m_cmdBuffer->vk())
    m_targetFB.reset();
    m_cmdBuffer.reset();
}

CVkRenderPass::~CVkRenderPass() = default;