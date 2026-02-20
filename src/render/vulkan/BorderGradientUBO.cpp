#include "BorderGradientUBO.hpp"
#include "Vulkan.hpp"
#include "utils.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

CVKBorderGradientUBO::CVKBorderGradientUBO(WP<CHyprVulkanDevice> device, VkDescriptorSetLayout layout) :
    IDeviceUser(device), m_dsPool(g_pHyprVulkan->allocateDescriptorSet(layout, &m_desctiptorSet, CVKDescriptorPool::DSP_UBO)) {

    if (!m_dsPool) {
        Log::logger->log(Log::ERR, "failed to allocate descriptor");
        return;
    }

    VkBufferCreateInfo bufferInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = sizeof(SVkBorderGradientShaderData),
        .usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateBuffer(vkDevice(), &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vkDevice(), m_buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        findVkMemType(m_device->physicalDevice(), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memRequirements.memoryTypeBits);

    if (vkAllocateMemory(vkDevice(), &allocInfo, nullptr, &m_bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(vkDevice(), m_buffer, m_bufferMemory, 0);

    VkDescriptorBufferInfo dsInfo = {
        .buffer = m_buffer,
        .offset = 0,
        .range  = sizeof(SVkBorderGradientShaderData),
    };

    VkWriteDescriptorSet dsWrite = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = m_desctiptorSet,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo     = &dsInfo,
    };

    vkUpdateDescriptorSets(vkDevice(), 1, &dsWrite, 0, nullptr);
}

void CVKBorderGradientUBO::update(const SVkBorderGradientShaderData& gradients) {
    void* uboData;
    vkMapMemory(vkDevice(), m_bufferMemory, 0, sizeof(SVkBorderGradientShaderData), 0, &uboData);
    memcpy(uboData, &gradients, sizeof(SVkBorderGradientShaderData));
    vkUnmapMemory(vkDevice(), m_bufferMemory);
}

VkDescriptorSet CVKBorderGradientUBO::ds() {
    return m_desctiptorSet;
}

CVKBorderGradientUBO::~CVKBorderGradientUBO() {
    if (m_desctiptorSet)
        vkFreeDescriptorSets(vkDevice(), m_dsPool->vkPool(), 1, &m_desctiptorSet);

    if (m_buffer)
        vkDestroyBuffer(vkDevice(), m_buffer, nullptr);

    if (m_bufferMemory)
        vkFreeMemory(vkDevice(), m_bufferMemory, nullptr);
}
