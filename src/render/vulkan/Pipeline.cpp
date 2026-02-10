#include "Pipeline.hpp"
#include "utils.hpp"
#include <vulkan/vulkan_core.h>

CVkPipeline::CVkPipeline(WP<CHyprVulkanDevice> device, WP<CVkRenderPass> renderPass, WP<CVkPipelineLayout> layout, WP<CVkShaders> shaders) :
    IDeviceUser(device), m_renderPass(renderPass), m_layout(layout) {

    VkSpecializationMapEntry specEntry = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof(uint32_t),
    };

    int                  colorTransformType = 0;

    VkSpecializationInfo specialization = {
        .mapEntryCount = 1,
        .pMapEntries   = &specEntry,
        .dataSize      = sizeof(uint32_t),
        .pData         = &colorTransformType,
    };

    VkPipelineShaderStageCreateInfo        stages[2]{{
                                                         .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                         .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                                                         .module = shaders->m_vert->module(),
                                                         .pName  = "main",
                                              },
                                                     {
                                                         .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                         .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .module              = shaders->m_frag->module(),
                                                         .pName               = "main",
                                                         .pSpecializationInfo = &specialization,
                                              }};

    VkPipelineInputAssemblyStateCreateInfo assembly = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_NONE,
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth   = 1.f,
    };

    VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable         = true,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &blendAttachment,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineViewportStateCreateInfo viewport = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    VkDynamicState dynStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = sizeof(dynStates) / sizeof(dynStates[0]),
        .pDynamicStates    = dynStates,
    };

    VkPipelineVertexInputStateCreateInfo vertex = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkGraphicsPipelineCreateInfo pinfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = sizeof(stages) / sizeof(stages[0]),
        .pStages             = stages,
        .pVertexInputState   = &vertex,
        .pInputAssemblyState = &assembly,
        .pViewportState      = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState   = &multisample,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dynamic,
        .layout              = m_layout->m_layout,
        .renderPass          = renderPass->m_vkRenderPass,
        .subpass             = 0,
    };

    VkPipelineCache cache = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(vkDevice(), cache, 1, &pinfo, nullptr, &m_vkPipeline) != VK_SUCCESS) {
        CRIT("failed to create vulkan pipelines");
    }
}

CVkPipeline::~CVkPipeline() {
    if (m_vkPipeline)
        vkDestroyPipeline(vkDevice(), m_vkPipeline, nullptr);
}