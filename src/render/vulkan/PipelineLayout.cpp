#include "PipelineLayout.hpp"
#include "utils.hpp"
#include <vulkan/vulkan_core.h>

CVkPipelineLayout::CVkPipelineLayout(WP<CHyprVulkanDevice> device, KEY key) : IDeviceUser(device), m_key({key}) {
    VkSamplerCreateInfo samplerCreateInfo = {
        .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter    = std::get<2>(key),
        .minFilter    = std::get<2>(key),
        .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod       = 0.f,
        .maxLod       = 0.25f,
    };

    // TODO YCC support
    // if (YCC) {
    //     VkSamplerYcbcrConversionCreateInfo conversionCreateInfo = {
    //         .sType         = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
    //         .format        = ,
    //         .ycbcrModel    = ,
    //         .ycbcrRange    = ,
    //         .xChromaOffset = VK_CHROMA_LOCATION_MIDPOINT,
    //         .yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT,
    //         .chromaFilter  = VK_FILTER_LINEAR,
    //     };
    //     if (vkCreateSamplerYcbcrConversion(vkDevice(), &conversionCreateInfo, NULL, &conversion) != VK_SUCCESS) {
    //         CRIT("vkCreateSamplerYcbcrConversion");
    //     }

    //     VkSamplerYcbcrConversionInfo conversionInfo = {
    //         .sType      = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
    //         .conversion = conversion,
    //     };
    //     samplerCreateInfo.pNext = &conversionInfo;
    // }

    if (vkCreateSampler(vkDevice(), &samplerCreateInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        CRIT("vkCreateSampler");
    }

    VkDescriptorSetLayoutBinding dsBinding = {
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = &m_sampler,
    };

    VkDescriptorSetLayoutCreateInfo dsInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &dsBinding,
    };

    if (vkCreateDescriptorSetLayout(vkDevice(), &dsInfo, nullptr, &m_descriptorSet) != VK_SUCCESS) {
        CRIT("vkCreateDescriptorSetLayout");
    }

    VkPushConstantRange pcRanges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .size       = std::get<0>(key),
        },
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = std::get<0>(key),
            .size       = std::get<1>(key),
        },
    };

    VkPipelineLayoutCreateInfo plInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &m_descriptorSet,
        .pushConstantRangeCount = 2,
        .pPushConstantRanges    = pcRanges,
    };

    if (vkCreatePipelineLayout(vkDevice(), &plInfo, nullptr, &m_layout) != VK_SUCCESS) {
        CRIT("vkCreatePipelineLayout");
    }
}

CVkPipelineLayout::KEY CVkPipelineLayout::key() {
    return m_key;
}

VkPipelineLayout CVkPipelineLayout::vk() {
    return m_layout;
}

VkDescriptorSetLayout CVkPipelineLayout::descriptorSet() {
    return m_descriptorSet;
}

CVkPipelineLayout::~CVkPipelineLayout() {
    if (m_layout)
        vkDestroyPipelineLayout(vkDevice(), m_layout, nullptr);

    if (m_descriptorSet)
        vkDestroyDescriptorSetLayout(vkDevice(), m_descriptorSet, nullptr);

    if (m_sampler)
        vkDestroySampler(vkDevice(), m_sampler, nullptr);
}
