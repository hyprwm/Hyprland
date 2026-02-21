#include "MemoryBuffer.hpp"
#include "MemorySpan.hpp"
#include "utils.hpp"
#include "../../debug/log/Logger.hpp"
#include <hyprutils/memory/SharedPtr.hpp>

SP<CVKMemoryBuffer> CVKMemoryBuffer::create(WP<CHyprVulkanDevice> device, VkDeviceSize bufferSize) {
    auto self    = makeShared<CVKMemoryBuffer>(device, bufferSize);
    self->m_self = self;
    return self;
}

CVKMemoryBuffer::CVKMemoryBuffer(WP<CHyprVulkanDevice> device, VkDeviceSize bufferSize) : IDeviceUser(device) {
    VkBufferCreateInfo bufInfo = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = bufferSize,
        .usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    IF_VKFAIL(vkCreateBuffer, vkDevice(), &bufInfo, nullptr, &m_buffer) {
        LOG_VKFAIL;
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(vkDevice(), m_buffer, &memReqs);

    const auto memTypeIndex = findVkMemType(m_device->physicalDevice(), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memReqs.memoryTypeBits);
    if (memTypeIndex < 0) {
        Log::logger->log(Log::ERR, "Failed to find memory type");
        return;
    }

    VkMemoryAllocateInfo memInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memTypeIndex,
    };
    IF_VKFAIL(vkAllocateMemory, vkDevice(), &memInfo, nullptr, &m_memory) {
        LOG_VKFAIL;
        return;
    }

    IF_VKFAIL(vkBindBufferMemory, vkDevice(), m_buffer, m_memory, 0) {
        LOG_VKFAIL;
        return;
    }

    IF_VKFAIL(vkMapMemory, vkDevice(), m_memory, 0, VK_WHOLE_SIZE, 0, &m_cpuMapping) {
        LOG_VKFAIL;
        return;
    }

    m_bufferSize = bufferSize;

    m_ok = true;
};

bool CVKMemoryBuffer::good() {
    return m_ok;
}

VkDeviceSize CVKMemoryBuffer::available() {
    return m_bufferSize - m_usedSize;
}

VkDeviceSize CVKMemoryBuffer::offset() {
    return m_usedSize;
}

SP<CVKMemorySpan> CVKMemoryBuffer::allocate(VkDeviceSize size, VkDeviceSize offset) {
    if (size + offset > available())
        return nullptr;

    const auto off = m_usedSize + offset;
    m_usedSize += size + offset;
    m_lastUsed.reset();
    return m_allocations.emplace_back(makeShared<CVKMemorySpan>(m_self, size, off));
}

void CVKMemoryBuffer::clear() {
    m_allocations.clear();
    m_usedSize = 0;
}

CVKMemoryBuffer::~CVKMemoryBuffer() {
    if (m_cpuMapping) {
        vkUnmapMemory(vkDevice(), m_memory);
        m_cpuMapping = nullptr;
    }

    if (m_buffer)
        vkDestroyBuffer(vkDevice(), m_buffer, nullptr);

    if (m_memory)
        vkFreeMemory(vkDevice(), m_memory, nullptr);
};
