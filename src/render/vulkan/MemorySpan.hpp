#pragma once

#include "MemoryBuffer.hpp"

class CVKMemorySpan {
  public:
    CVKMemorySpan(WP<CVKMemoryBuffer> buffer, VkDeviceSize size, VkDeviceSize offset);
    // ~CVKMemorySpan();

    struct SAllocation {
        VkDeviceSize start;
        VkDeviceSize size;
    };

    char*        cpuMapping();
    VkDeviceSize size();
    VkDeviceSize offset();
    VkBuffer     vkBuffer();

  private:
    WP<CVKMemoryBuffer> m_buffer;
    VkDeviceSize        m_size;
    VkDeviceSize        m_offset;

    friend class CVKMemoryBuffer;
};
