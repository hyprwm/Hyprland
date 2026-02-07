#pragma once

#include <cstdint>
#include <hyprutils/math/Vector2D.hpp>
#include <set>
#include <vector>
#include <vulkan/vulkan.h>

#include "../helpers/memory/Memory.hpp"
#include "../helpers/Format.hpp"

class CHyprVulkanImpl;
class CHyprVulkanDevice;

struct SVkFormatModifier {
    VkDrmFormatModifierPropertiesEXT props;
    VkExtent2D                       maxExtent;
    bool                             canSrgb = false;
};

struct SVkFormatProps {
    SPixelFormat format;

    struct {
        VkExtent2D           maxExtent;
        VkFormatFeatureFlags features;
        bool                 canSrgb = false;
    } shm;

    struct {
        std::vector<SVkFormatModifier> renderModifiers;
        std::vector<SVkFormatModifier> textureModifiers;
    } dmabuf;

    bool hasSrgb() const {
        return format.vkSrgbFormat != VK_FORMAT_UNDEFINED;
    }
};

class CHyprVulkanImpl {
  public:
    CHyprVulkanImpl();
    ~CHyprVulkanImpl();

    struct {
        PFN_vkEnumerateInstanceVersion      pfEnumInstanceVersion           = nullptr;
        PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT  = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
    } m_proc;

    UP<CHyprVulkanDevice> getDevice(int drmFd);

  private:
    inline void              loadVulkanProc(void* pProc, const char* name);

    VkInstance               m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug    = VK_NULL_HANDLE;
    UP<CHyprVulkanDevice>    m_device;
};

class CHyprVulkanDevice {
  public:
    CHyprVulkanDevice(VkPhysicalDevice device, std::vector<VkExtensionProperties> extensions);
    ~CHyprVulkanDevice();

    struct {
        PFN_vkGetMemoryFdPropertiesKHR    vkGetMemoryFdPropertiesKHR    = nullptr;
        PFN_vkWaitSemaphoresKHR           vkWaitSemaphoresKHR           = nullptr;
        PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR = nullptr;
        PFN_vkQueueSubmit2KHR             vkQueueSubmit2KHR             = nullptr;
        PFN_vkGetSemaphoreFdKHR           vkGetSemaphoreFdKHR           = nullptr;
        PFN_vkImportSemaphoreFdKHR        vkImportSemaphoreFdKHR        = nullptr;
    } m_proc;

    bool good();
    void loadFormats();

  private:
    inline void                            loadVulkanProc(void* pProc, const char* name);
    bool                                   getFormat(const SPixelFormat& format);
    std::optional<VkImageFormatProperties> getSupportedSHMProperties(VkFormat vkFormat, VkFormat vkSrgbFormat);
    std::optional<SVkFormatModifier>   getSupportedDRMProperties(VkFormat vkFormat, VkFormat vkSrgbFormat, VkImageUsageFlags usage, const VkDrmFormatModifierPropertiesEXT& mod);
    bool                               getModifiers(SVkFormatProps& props, const size_t modifierCount);

    VkPhysicalDevice                   m_physicalDevice;
    VkDevice                           m_device;
    VkQueue                            m_queue;
    std::vector<VkExtensionProperties> m_extensions;
    int                                m_drmFd = -1;

    bool                               m_good = false;

    uint32_t                           m_queueFamilyIndex = 0;
    bool                               m_canExplicitSync  = false;
    bool                               m_canConvertYCC    = false;
    std::set<DRMFormat>                m_shmTextureFormats;
    std::set<DRMFormat>                m_dmaRenderFormats;
    std::set<DRMFormat>                m_dmaTextureFormats;
    std::vector<SVkFormatProps>        m_formats;
};

inline UP<CHyprVulkanImpl> g_pHyprVulkan;
