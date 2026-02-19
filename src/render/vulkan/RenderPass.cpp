#include "RenderPass.hpp"
#include "DeviceUser.hpp"
#include "helpers/Format.hpp"
#include "macros.hpp"
#include "render/vulkan/types.hpp"
#include "utils.hpp"
#include <hyprutils/memory/Casts.hpp>

CVkRenderPass::CVkRenderPass(WP<CHyprVulkanDevice> device, DRMFormat format, SP<CVkShaders> shaders) : IDeviceUser(device), m_drmFormat(format), m_shaders(shaders) {
    const auto info = m_device->getFormat(m_drmFormat);
    RASSERT(info.has_value(), "No info for drm format {}", NFormatUtils::drmFormatName(m_drmFormat));

    VkAttachmentDescription attachment = {
        .format         = info->format.vkFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkAttachmentReference colorRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorRef,
    };

    VkSubpassDependency deps[] = {
        {
            .srcSubpass    = VK_SUBPASS_EXTERNAL,
            .dstSubpass    = 0,
            .srcStageMask  = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
        },
        {
            .srcSubpass    = 0,
            .dstSubpass    = VK_SUBPASS_EXTERNAL,
            .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,
        },
    };

    VkRenderPassCreateInfo rpInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &attachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = sizeof(deps) / sizeof(deps[0]),
        .pDependencies   = deps,
    };

    if (vkCreateRenderPass(vkDevice(), &rpInfo, nullptr, &m_vkRenderPass) != VK_SUCCESS) {
        CRIT("Failed to create render pass");
    }
}

WP<CVkPipeline> CVkRenderPass::borderPipeline() {
    if (!m_borderPipeline)
        m_borderPipeline = makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_border);

    return m_borderPipeline;
}

WP<CVkPipeline> CVkRenderPass::rectPipeline() {
    if (!m_rectPipeline)
        m_rectPipeline = makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_rect);

    return m_rectPipeline;
}

WP<CVkPipeline> CVkRenderPass::shadowPipeline() {
    if (!m_shadowPipeline)
        m_shadowPipeline = makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_shadow);

    return m_shadowPipeline;
}

WP<CVkPipeline> CVkRenderPass::texturePipeline() {
    if (!m_texturePipeline)
        m_texturePipeline = makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_frag);

    return m_texturePipeline;
}

WP<CVkPipeline> CVkRenderPass::textureMattePipeline() {
    if (!m_textureMattePipeline)
        m_textureMattePipeline = makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_matte, 2);

    return m_textureMattePipeline;
}

WP<CVkPipeline> CVkRenderPass::passPipeline() {
    if (!m_passPipeline)
        m_passPipeline = makeShared<CVkPipeline>(m_device, m_vkRenderPass, m_shaders->m_vert, m_shaders->m_pass);

    return m_passPipeline;
}

CVkRenderPass::~CVkRenderPass() {
    if (m_vkRenderPass)
        vkDestroyRenderPass(vkDevice(), m_vkRenderPass, nullptr);
};