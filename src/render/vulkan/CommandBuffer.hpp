#pragma once

#include "Device.hpp"

#include "../../helpers/memory/Memory.hpp"
#include "DeviceUser.hpp"
#include <vulkan/vulkan_core.h>

class CHyprVKRenderer;

class CHyprVkCommandBuffer : public IDeviceUser {
  public:
    CHyprVkCommandBuffer(WP<CHyprVulkanDevice> device);
    ~CHyprVkCommandBuffer();

    void begin();
    void end(uint64_t signalPoint);

  private:
    VkCommandPool   m_cmdPool         = VK_NULL_HANDLE;
    VkCommandBuffer m_cmdBuffer       = VK_NULL_HANDLE;
    VkSemaphore     m_waitSemaphore   = VK_NULL_HANDLE;
    VkSemaphore     m_signalSemaphore = VK_NULL_HANDLE;

    bool            m_recording = false;

    friend class CHyprVKRenderer;
};