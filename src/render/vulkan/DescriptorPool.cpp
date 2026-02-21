#include "DescriptorPool.hpp"
#include "../../debug/log/Logger.hpp"
#include "utils.hpp"
#include <vulkan/vulkan_core.h>

CVKDescriptorPool::CVKDescriptorPool(WP<CHyprVulkanDevice> device, VkDescriptorType type, size_t size) : IDeviceUser(device), m_available(size) {
    VkDescriptorPoolSize poolSize = {
        .type            = type,
        .descriptorCount = size,
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = size,
        .poolSizeCount = 1,
        .pPoolSizes    = &poolSize,
    };

    IF_VKFAIL(vkCreateDescriptorPool, vkDevice(), &poolInfo, nullptr, &m_pool) {
        LOG_VKFAIL;
        return;
    }
}

VkResult CVKDescriptorPool::allocateSet(VkDescriptorSetLayout layout, VkDescriptorSet* ds) {
    if (!m_available)
        return VK_ERROR_OUT_OF_POOL_MEMORY;

    VkDescriptorSetAllocateInfo dsInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = m_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &layout,
    };

    const auto res = vkAllocateDescriptorSets(vkDevice(), &dsInfo, ds);
    if (res == VK_SUCCESS)
        --m_available;
    return res;
}

VkDescriptorPool CVKDescriptorPool::vkPool() {
    return m_pool;
}

CVKDescriptorPool::~CVKDescriptorPool() {
    if (m_pool)
        vkDestroyDescriptorPool(vkDevice(), m_pool, nullptr);
}
