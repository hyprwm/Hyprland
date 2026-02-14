#pragma once

#include "../../helpers/time/Timer.hpp"
#include "DeviceUser.hpp"
#include <vector>
#include <vulkan/vulkan_core.h>

class CVKMemorySpan;

class CVKMemoryBuffer : public IDeviceUser {
  public:
    static SP<CVKMemoryBuffer> create(WP<CHyprVulkanDevice> device, VkDeviceSize bufferSize);
    CVKMemoryBuffer(WP<CHyprVulkanDevice> device, VkDeviceSize bufferSize);
    ~CVKMemoryBuffer();

    bool              good();
    VkDeviceSize      available();
    VkDeviceSize      offset();
    SP<CVKMemorySpan> allocate(VkDeviceSize size, VkDeviceSize offset = 0);
    void              clear();

  private:
    WP<CVKMemoryBuffer>            m_self;
    bool                           m_ok = false;

    VkBuffer                       m_buffer;
    VkDeviceMemory                 m_memory;
    VkDeviceSize                   m_bufferSize = 0;
    VkDeviceSize                   m_usedSize   = 0;
    void*                          m_cpuMapping = nullptr;
    CTimer                         m_lastUsed;

    std::vector<SP<CVKMemorySpan>> m_allocations;

    friend class CVKMemorySpan;
    friend class CHyprVulkanImpl;
};
