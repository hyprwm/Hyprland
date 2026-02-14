#pragma once

#include <cstdint>
#include <hyprutils/math/Vector2D.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "../../helpers/memory/Memory.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"
#include "render/vulkan/DescriptorPool.hpp"
#include "render/vulkan/MemoryBuffer.hpp"

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

    SP<CHyprVulkanDevice>    device();
    VkDevice                 vkDevice();
    WP<CHyprVkCommandBuffer> renderCB();
    WP<CHyprVkCommandBuffer> stageCB();
    WP<CVKMemorySpan>        getMemorySpan(VkDeviceSize size, VkDeviceSize alignment = 1);
    SP<CVKDescriptorPool>    allocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet* ds);

  private:
    inline void                           loadVulkanProc(void* pProc, const char* name);
    bool                                  waitCommandBuffer(WP<CHyprVkCommandBuffer>);
    SP<CHyprVkCommandBuffer>              acquireCB();

    VkInstance                            m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT              m_debug    = VK_NULL_HANDLE;

    SP<CHyprVulkanDevice>                 m_device;
    SP<CHyprVkCommandBuffer>              m_currentStageCB;
    SP<CHyprVkCommandBuffer>              m_currentRenderCB;
    std::vector<SP<CHyprVkCommandBuffer>> m_commandBuffers;
    std::vector<SP<CVKMemoryBuffer>>      m_sharedBuffers;
    std::vector<SP<CVKDescriptorPool>>    m_dsPools;
    size_t                                m_lastDsPoolSize = 0;

    uint64_t                              m_lastStagePoint = 0;

    friend class CHyprVKRenderer;
};

inline UP<CHyprVulkanImpl> g_pHyprVulkan;
