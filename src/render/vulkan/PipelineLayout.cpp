#include "PipelineLayout.hpp"
#include "utils.hpp"
#include <vulkan/vulkan_core.h>

using namespace Render::VK;

CVkPipelineLayout::CVkPipelineLayout(WP<CHyprVulkanDevice> device, KEY key) : IDeviceUser(device), m_key({key}) {
    const auto [vertSize, fragSize, filter, texCount, uboCount] = key;

    VkSamplerCreateInfo samplerCreateInfo = {
        .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter    = filter,
        .minFilter    = filter,
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

    m_descriptorSets.resize(std::max((int)texCount, 1));
    for (int i = 0; i < std::max((int)texCount, 1); i++) {
        const auto                                tc = texCount > 0 ? 1 : 0;
        std::vector<VkDescriptorSetLayoutBinding> dsBinding(tc + uboCount);
        if (tc > 0) {
            dsBinding[0] = {
                .binding            = 0,
                .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount    = 1,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = &m_sampler,
            };
        }
        for (int i = 0; i < uboCount; i++) {
            dsBinding[tc + i] = {
                .binding         = tc + i,
                .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
            };
        }

        VkDescriptorSetLayoutCreateInfo dsInfo = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = dsBinding.size(),
            .pBindings    = dsBinding.data(),
        };

        if (vkCreateDescriptorSetLayout(vkDevice(), &dsInfo, nullptr, &m_descriptorSets[i]) != VK_SUCCESS) {
            CRIT("vkCreateDescriptorSetLayout");
        }
    }

    VkPushConstantRange pcRanges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .size       = vertSize,
        },
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = vertSize,
            .size       = fragSize,
        },
    };

    VkPipelineLayoutCreateInfo plInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = m_descriptorSets.size(),
        .pSetLayouts            = m_descriptorSets.data(),
        .pushConstantRangeCount = pcRanges[1].size > 0 ? 2 : 1,
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

std::vector<VkDescriptorSetLayout> CVkPipelineLayout::descriptorSets() {
    return m_descriptorSets;
}

CVkPipelineLayout::~CVkPipelineLayout() {
    if (m_layout)
        vkDestroyPipelineLayout(vkDevice(), m_layout, nullptr);

    for (const auto& ds : m_descriptorSets)
        vkDestroyDescriptorSetLayout(vkDevice(), ds, nullptr);

    if (m_sampler)
        vkDestroySampler(vkDevice(), m_sampler, nullptr);
}
