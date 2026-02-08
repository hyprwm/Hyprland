#pragma once

#include <hyprutils/math/Vector2D.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "../../helpers/memory/Memory.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"

class CHyprVulkanImpl {
  public:
    CHyprVulkanImpl();
    ~CHyprVulkanImpl();

    struct {
        PFN_vkEnumerateInstanceVersion      pfEnumInstanceVersion           = nullptr;
        PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT  = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
    } m_proc;

    SP<CHyprVulkanDevice>    getDevice(int drmFd);
    SP<CHyprVkCommandBuffer> begin();
    void                     end();

  private:
    inline void              loadVulkanProc(void* pProc, const char* name);

    VkInstance               m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug    = VK_NULL_HANDLE;

    SP<CHyprVulkanDevice>    m_device;
    SP<CHyprVkCommandBuffer> m_commandBuffer;
};

inline UP<CHyprVulkanImpl> g_pHyprVulkan;
