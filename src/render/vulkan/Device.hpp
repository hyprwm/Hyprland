#pragma once

#include <cstdint>
#include <set>
#include <vulkan/vulkan.h>
#include "helpers/Format.hpp"
#include "types.hpp"

class CHyprVulkanDevice {
  public:
    CHyprVulkanDevice(VkPhysicalDevice device, std::vector<VkExtensionProperties> extensions);
    ~CHyprVulkanDevice();

    struct {
        PFN_vkGetMemoryFdPropertiesKHR     vkGetMemoryFdPropertiesKHR     = nullptr;
        PFN_vkWaitSemaphoresKHR            vkWaitSemaphoresKHR            = nullptr;
        PFN_vkGetSemaphoreCounterValueKHR  vkGetSemaphoreCounterValueKHR  = nullptr;
        PFN_vkGetSemaphoreFdKHR            vkGetSemaphoreFdKHR            = nullptr;
        PFN_vkImportSemaphoreFdKHR         vkImportSemaphoreFdKHR         = nullptr;
        PFN_vkSetDebugUtilsObjectNameEXT   vkSetDebugUtilsObjectNameEXT   = nullptr;
        PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT = nullptr;
        PFN_vkQueueEndDebugUtilsLabelEXT   vkQueueEndDebugUtilsLabelEXT   = nullptr;
        PFN_vkCmdBeginDebugUtilsLabelEXT   vkCmdBeginDebugUtilsLabelEXT   = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT     vkCmdEndDebugUtilsLabelEXT     = nullptr;

    } m_proc;

    bool                                good();
    void                                loadFormats();
    VkDevice                            vkDevice();
    VkPhysicalDevice                    physicalDevice();

    uint32_t                            queueFamilyIndex();
    VkQueue                             queue();
    VkSemaphore                         timelineSemaphore();
    uint64_t                            timelinePoint();
    uint64_t                            m_timelinePoint = 0;

    std::optional<const SVkFormatProps> getFormat(const DRMFormat format);
    VkCommandPool                       commandPool();
    const std::vector<SVkFormatProps>&  formats();

  private:
    inline void                            loadVulkanProc(void* pProc, const char* name);
    std::optional<SVkFormatProps>          getFormat(const SPixelFormat& format);
    std::optional<VkImageFormatProperties> getSupportedSHMProperties(VkFormat vkFormat, VkFormat vkSrgbFormat);
    std::optional<SVkFormatModifier>   getSupportedDRMProperties(VkFormat vkFormat, VkFormat vkSrgbFormat, VkImageUsageFlags usage, const VkDrmFormatModifierPropertiesEXT& mod);
    bool                               getModifiers(SVkFormatProps& props, const size_t modifierCount);

    VkPhysicalDevice                   m_physicalDevice    = VK_NULL_HANDLE;
    VkDevice                           m_device            = VK_NULL_HANDLE;
    VkQueue                            m_queue             = VK_NULL_HANDLE;
    VkSemaphore                        m_timelineSemaphore = VK_NULL_HANDLE;
    VkCommandPool                      m_commandPool       = VK_NULL_HANDLE;

    int                                m_drmFd = -1;
    std::vector<VkExtensionProperties> m_extensions;

    bool                               m_good = false;

    uint32_t                           m_queueFamilyIndex = 0;
    bool                               m_canExplicitSync  = false;
    bool                               m_canConvertYCC    = false;
    std::set<DRMFormat>                m_shmTextureFormats;
    std::set<DRMFormat>                m_dmaRenderFormats;
    std::set<DRMFormat>                m_dmaTextureFormats;
    std::vector<SVkFormatProps>        m_formats;
};