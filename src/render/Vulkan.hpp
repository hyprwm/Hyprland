#pragma once

#include <vulkan/vulkan.h>

#include "../helpers/memory/Memory.hpp"

class CHyprVulkanImpl {
  public:
    CHyprVulkanImpl();
    ~CHyprVulkanImpl();

    struct {
        PFN_vkEnumerateInstanceVersion      pfEnumInstanceVersion           = nullptr;
        PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT  = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
    } m_proc;

    VkPhysicalDevice getDevice(int drmFd);

  private:
    inline void              loadVulkanProc(void* pProc, const char* name);

    VkInstance               m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug    = VK_NULL_HANDLE;
};

inline UP<CHyprVulkanImpl> g_pHyprVulkan;
