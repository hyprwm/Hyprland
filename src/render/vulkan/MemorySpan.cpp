#include "MemorySpan.hpp"

CVKMemorySpan::CVKMemorySpan(WP<CVKMemoryBuffer> buffer, VkDeviceSize size, VkDeviceSize offset) : m_buffer(buffer), m_size(size), m_offset(offset) {}

char* CVKMemorySpan::cpuMapping() {
    return (char*)m_buffer->m_cpuMapping + m_offset;
}

VkDeviceSize CVKMemorySpan::size() {
    return m_size;
}

VkDeviceSize CVKMemorySpan::offset() {
    return m_offset;
}

VkBuffer CVKMemorySpan::vkBuffer() {
    return m_buffer->m_buffer;
}
