#include "Device.hpp"
#include "helpers/Format.hpp"
#include "utils.hpp"

#include "../../debug/log/Logger.hpp"
#include "../../macros.hpp"
#include <algorithm>
#include <optional>

inline void CHyprVulkanDevice::loadVulkanProc(void* pProc, const char* name) {
    void* proc = rc<void*>(vkGetDeviceProcAddr(m_device, name));
    if (proc == nullptr) {
        CRIT("[VULKAN] vkGetDeviceProcAddr({}) failed", name);
    }
    *sc<void**>(pProc) = proc;
}

CHyprVulkanDevice::CHyprVulkanDevice(VkPhysicalDevice device, std::vector<VkExtensionProperties> extensions) : m_physicalDevice(device), m_extensions(std::move(extensions)) {
    for (const auto& extension : m_extensions) {
        Log::logger->log(Log::DEBUG, "Vulkan device extension {} v{}", extension.extensionName, extension.specVersion);
    }

    std::vector<const char*> enabledExtensions = {
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,   VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,         VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    };

    for (const auto& ext : enabledExtensions) {
        if (!hasExtension(m_extensions, ext)) {
            Log::logger->log(Log::ERR, "[VULKAN] Required device extension {} not found", ext);
            return;
        }
    }

    uint32_t queuePropCount;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queuePropCount, nullptr);
    ASSERT(queuePropCount > 0);
    std::vector<VkQueueFamilyProperties> queueProps(queuePropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queuePropCount, queueProps.data());

    const auto prop = std::ranges::find_if(queueProps, [](const auto& prop) { return prop.queueFlags & VK_QUEUE_GRAPHICS_BIT; });
    ASSERT(prop != queueProps.end());
    m_queueFamilyIndex = prop - queueProps.begin();

    const bool hasExternalSemaphoreFd = hasExtension(m_extensions, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    if (hasExternalSemaphoreFd) {
        const VkPhysicalDeviceExternalSemaphoreInfo extSemaphoreInfo = {
            .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
            .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
        };
        VkExternalSemaphoreProperties extSemaphoreProps = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
        };
        vkGetPhysicalDeviceExternalSemaphoreProperties(m_physicalDevice, &extSemaphoreInfo, &extSemaphoreProps);
        m_canExplicitSync = extSemaphoreProps.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT &&
            extSemaphoreProps.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
        enabledExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    }

    if (!m_canExplicitSync)
        Log::logger->log(Log::DEBUG, "VkSemaphore is not usable as sync_file");

    VkPhysicalDeviceSamplerYcbcrConversionFeatures YCCfeature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &YCCfeature,
    };
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features);

    m_canConvertYCC = YCCfeature.samplerYcbcrConversion;
    Log::logger->log(Log::DEBUG, "Sampler YCC conversion: {}", m_canConvertYCC);

    const float             prio  = 1.f;
    VkDeviceQueueCreateInfo qinfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_queueFamilyIndex,
        .queueCount       = 1,
        .pQueuePriorities = &prio,
    };

    const bool                               hasGlobalPriority = hasExtension(m_extensions, VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME);
    VkDeviceQueueGlobalPriorityCreateInfoKHR globalPriority;
    if (hasGlobalPriority) {
        // If global priorities are supported, request a high-priority context
        globalPriority = VkDeviceQueueGlobalPriorityCreateInfoKHR{
            .sType          = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR,
            .globalPriority = VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR,
        };
        qinfo.pNext = &globalPriority;
        enabledExtensions.push_back(VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME);
        Log::logger->log(Log::DEBUG, "Requesting a high-priority device queue");
    } else
        Log::logger->log(Log::DEBUG, "Requesting a regular device queue");

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features = {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
        .pNext            = &YCCfeature,
        .synchronization2 = VK_TRUE,
    };
    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timelineFeatures = {
        .sType             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
        .pNext             = &sync2Features,
        .timelineSemaphore = VK_TRUE,
    };
    VkDeviceCreateInfo devInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &timelineFeatures,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &qinfo,
        .enabledExtensionCount   = enabledExtensions.size(),
        .ppEnabledExtensionNames = enabledExtensions.data(),
    };

    VkResult res = vkCreateDevice(m_physicalDevice, &devInfo, nullptr, &m_device);

    if (hasGlobalPriority && (res == VK_ERROR_NOT_PERMITTED_EXT || res == VK_ERROR_INITIALIZATION_FAILED)) {
        // Try to recover from the driver denying a global priority queue
        Log::logger->log(Log::DEBUG, "Failed to obtain a high-priority device queue, falling back to regular queue");
        qinfo.pNext = nullptr;
        res         = vkCreateDevice(m_physicalDevice, &devInfo, nullptr, &m_device);
    }

    if (res != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "Failed to create vulkan device: {}", resultToStr(res));
        return;
    }

    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

    loadVulkanProc(&m_proc.vkGetMemoryFdPropertiesKHR, "vkGetMemoryFdPropertiesKHR");
    loadVulkanProc(&m_proc.vkWaitSemaphoresKHR, "vkWaitSemaphoresKHR");
    loadVulkanProc(&m_proc.vkGetSemaphoreCounterValueKHR, "vkGetSemaphoreCounterValueKHR");

    if (m_canExplicitSync) {
        loadVulkanProc(&m_proc.vkGetSemaphoreFdKHR, "vkGetSemaphoreFdKHR");
        loadVulkanProc(&m_proc.vkImportSemaphoreFdKHR, "vkImportSemaphoreFdKHR");
    }

#if ISDEBUG

    loadVulkanProc(&m_proc.vkSetDebugUtilsObjectNameEXT, "vkSetDebugUtilsObjectNameEXT");
    loadVulkanProc(&m_proc.vkQueueBeginDebugUtilsLabelEXT, "vkQueueBeginDebugUtilsLabelEXT");
    loadVulkanProc(&m_proc.vkQueueEndDebugUtilsLabelEXT, "vkQueueEndDebugUtilsLabelEXT");
    loadVulkanProc(&m_proc.vkCmdBeginDebugUtilsLabelEXT, "vkCmdBeginDebugUtilsLabelEXT");
    loadVulkanProc(&m_proc.vkCmdEndDebugUtilsLabelEXT, "vkCmdEndDebugUtilsLabelEXT");
#endif

    VkSemaphoreTypeCreateInfo timelineCreateInfo = {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext         = nullptr,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue  = m_timelinePoint,
    };

    const VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineCreateInfo,
    };

    if (vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_timelineSemaphore) != VK_SUCCESS) {
        CRIT("vkCreateSemaphore failed");
    }

    const VkCommandPoolCreateInfo cmdPoolCreateInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex(),
    };

    if (vkCreateCommandPool(vkDevice(), &cmdPoolCreateInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        CRIT("vkCreateCommandPool failed");
    }

    m_good = true;
}

bool CHyprVulkanDevice::good() {
    return m_good;
}

VkDevice CHyprVulkanDevice::vkDevice() {
    return m_device;
}

VkPhysicalDevice CHyprVulkanDevice::physicalDevice() {
    return m_physicalDevice;
}

uint32_t CHyprVulkanDevice::queueFamilyIndex() {
    return m_queueFamilyIndex;
}

VkQueue CHyprVulkanDevice::queue() {
    return m_queue;
}

VkSemaphore CHyprVulkanDevice::timelineSemaphore() {
    return m_timelineSemaphore;
}

uint64_t CHyprVulkanDevice::timelinePoint() {
    uint64_t point;
    IF_VKFAIL(vkGetSemaphoreCounterValue, vkDevice(), m_timelineSemaphore, &point) {
        LOG_VKFAIL;
        return 0;
    }
    return point;
}

VkCommandPool CHyprVulkanDevice::commandPool() {
    return m_commandPool;
}

const std::vector<SVkFormatProps>& CHyprVulkanDevice::formats() {
    return m_formats;
}

std::optional<const SVkFormatProps> CHyprVulkanDevice::getFormat(const DRMFormat format) {
    const auto found = std::ranges::find_if(m_formats, [format](const auto fmt) { return fmt.format.drmFormat == format; });
    if (found != m_formats.end())
        return *found;

    return {};
}

void CHyprVulkanDevice::loadFormats() {
    for (const auto& format : KNOWN_FORMATS) {
        const auto props = getFormat(format);
        if (props.has_value()) {
            Log::logger->log(Log::DEBUG, "Loaded format {}", NFormatUtils::drmFormatName(format.drmFormat));
            m_formats.emplace_back(props.value());
        }
    }
}

std::optional<SVkFormatProps> CHyprVulkanDevice::getFormat(const SPixelFormat& format) {
    if (format.isYCC && !m_canConvertYCC)
        return {};

    VkDrmFormatModifierPropertiesListEXT mods = {
        .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };
    VkFormatProperties2 formatProps = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &mods,
    };

    vkGetPhysicalDeviceFormatProperties2(m_physicalDevice, format.vkFormat, &formatProps);

    bool           isValid = false;
    SVkFormatProps props   = {.format = format};

    if ((formatProps.formatProperties.optimalTilingFeatures & SHM_TEX_FEATURES) == SHM_TEX_FEATURES && !format.isYCC) {
        bool canSrgb = false;

        auto supportedProps = getSupportedSHMProperties(format.vkFormat, format.vkSrgbFormat);
        if (supportedProps.has_value())
            canSrgb = props.hasSrgb();
        else if (props.hasSrgb())
            supportedProps = getSupportedSHMProperties(format.vkFormat, VK_FORMAT_UNDEFINED);

        if (supportedProps.has_value()) {
            props.shm.maxExtent.width  = supportedProps->maxExtent.width;
            props.shm.maxExtent.height = supportedProps->maxExtent.height;
            props.shm.features         = formatProps.formatProperties.optimalTilingFeatures;
            props.shm.canSrgb          = canSrgb;
            m_shmTextureFormats.insert(format.drmFormat);

            isValid = true;
        }
    } else
        Log::logger->log(Log::DEBUG, "{} missing required features", NFormatUtils::drmFormatName(format.drmFormat));

    if (mods.drmFormatModifierCount > 0)
        isValid |= getModifiers(props, mods.drmFormatModifierCount);

    if (isValid)
        return props;

    return {};
}

std::optional<VkImageFormatProperties> CHyprVulkanDevice::getSupportedSHMProperties(VkFormat vkFormat, VkFormat vkSrgbFormat) {
    VkFormat viewFormats[2] = {
        vkFormat,
        vkSrgbFormat,
    };
    VkImageFormatListCreateInfoKHR listi = {
        .sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
        .pNext           = nullptr,
        .viewFormatCount = vkSrgbFormat != VK_FORMAT_UNDEFINED ? 2 : 1,
        .pViewFormats    = viewFormats,
    };
    VkPhysicalDeviceImageFormatInfo2 formatInfo = {
        .sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext  = &listi,
        .format = vkFormat,
        .type   = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage  = VULKAN_SHM_TEX_USAGE,
        .flags  = vkSrgbFormat != VK_FORMAT_UNDEFINED ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0,
    };
    VkImageFormatProperties2 propInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
    };

    const auto res = vkGetPhysicalDeviceImageFormatProperties2(m_physicalDevice, &formatInfo, &propInfo);
    if (res != VK_SUCCESS) {
        if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
            Log::logger->log(Log::ERR, "Unsupported format {} ({})", (uint32_t)vkFormat, (uint32_t)vkSrgbFormat);
        else
            Log::logger->log(Log::ERR, "vkGetPhysicalDeviceImageFormatProperties2: {}", resultToStr(res));

        return {};
    }

    return propInfo.imageFormatProperties;
}

bool CHyprVulkanDevice::getModifiers(SVkFormatProps& props, const size_t modifierCount) {
    VkDrmFormatModifierPropertiesListEXT modProps = {
        .sType                  = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
        .drmFormatModifierCount = modifierCount,
    };
    VkFormatProperties2 formatProps = {
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        .pNext = &modProps,
    };

    std::vector<VkDrmFormatModifierPropertiesEXT> drmFormatModifierProperties(modifierCount);
    modProps.pDrmFormatModifierProperties = drmFormatModifierProperties.data();

    vkGetPhysicalDeviceFormatProperties2(m_physicalDevice, props.format.vkFormat, &formatProps);

    props.dmabuf.renderModifiers.resize(modProps.drmFormatModifierCount);
    props.dmabuf.textureModifiers.resize(modProps.drmFormatModifierCount);

    bool found = false;
    for (const auto& mod : drmFormatModifierProperties) {
        if ((mod.drmFormatModifierTilingFeatures & RENDER_FEATURES) == RENDER_FEATURES && !props.format.isYCC) {
            auto modProps = getSupportedDRMProperties(props.format.vkFormat, props.format.vkSrgbFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, mod);

            if (modProps.has_value())
                modProps->canSrgb = props.hasSrgb();
            else if (props.hasSrgb())
                modProps = getSupportedDRMProperties(props.format.vkFormat, VK_FORMAT_UNDEFINED, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, mod);

            if (modProps.has_value()) {
                props.dmabuf.renderModifiers.emplace_back(modProps.value());
                m_dmaRenderFormats.insert(props.format.drmFormat);
                found = true;
            }
        } else
            Log::logger->log(Log::DEBUG, "{}({}) missing required render features", NFormatUtils::drmFormatName(props.format.drmFormat),
                             NFormatUtils::drmModifierName(mod.drmFormatModifier));

        VkFormatFeatureFlags features = DMA_TEX_FEATURES;
        if (props.format.isYCC)
            features |= YCC_TEX_FEATURES;

        if ((mod.drmFormatModifierTilingFeatures & features) == features) {
            auto modProps = getSupportedDRMProperties(props.format.vkFormat, props.format.vkSrgbFormat, VULKAN_DMA_TEX_USAGE, mod);
            if (modProps.has_value())
                modProps->canSrgb = props.hasSrgb();
            else if (props.hasSrgb())
                modProps = getSupportedDRMProperties(props.format.vkFormat, VK_FORMAT_UNDEFINED, VULKAN_DMA_TEX_USAGE, mod);

            if (modProps.has_value()) {
                props.dmabuf.textureModifiers.emplace_back(modProps.value());
                m_dmaTextureFormats.insert(props.format.drmFormat);
                found = true;
            }
        } else
            Log::logger->log(Log::ERR, "{}({}) missing required texture features", NFormatUtils::drmFormatName(props.format.drmFormat),
                             NFormatUtils::drmModifierName(mod.drmFormatModifier));
    }

    return found;
}

std::optional<SVkFormatModifier> CHyprVulkanDevice::getSupportedDRMProperties(VkFormat vkFormat, VkFormat vkSrgbFormat, VkImageUsageFlags usage,
                                                                              const VkDrmFormatModifierPropertiesEXT& mod) {
    VkFormat view_formats[2] = {
        vkFormat,
        vkSrgbFormat,
    };
    VkImageFormatListCreateInfoKHR listi = {
        .sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
        .viewFormatCount = vkSrgbFormat != VK_FORMAT_UNDEFINED ? 2 : 1,
        .pViewFormats    = view_formats,
    };
    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modi = {
        .sType             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
        .pNext             = &listi,
        .drmFormatModifier = mod.drmFormatModifier,
        .sharingMode       = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkPhysicalDeviceExternalImageFormatInfo efmti = {
        .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .pNext      = &modi,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkPhysicalDeviceImageFormatInfo2 fmti = {
        .sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext  = &efmti,
        .format = vkFormat,
        .type   = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
        .usage  = usage,
        .flags  = vkSrgbFormat != VK_FORMAT_UNDEFINED ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0,
    };

    VkExternalImageFormatProperties efmtp = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };
    VkImageFormatProperties2 ifmtp = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &efmtp,
    };
    const VkExternalMemoryProperties* emp = &efmtp.externalMemoryProperties;

    const auto                        res = vkGetPhysicalDeviceImageFormatProperties2(m_physicalDevice, &fmti, &ifmtp);

    if (res != VK_SUCCESS) {
        if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
            Log::logger->log(Log::ERR, "Unsupported format {} ({})", (uint32_t)vkFormat, (uint32_t)vkSrgbFormat);
        else
            Log::logger->log(Log::ERR, "vkGetPhysicalDeviceImageFormatProperties2: {}", resultToStr(res));

        return {};
    } else if (!(emp->externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) {
        Log::logger->log(Log::ERR, "Format {} ({}) doesn't support import", (uint32_t)vkFormat, (uint32_t)vkSrgbFormat);
        return {};
    }

    VkExtent3D me = ifmtp.imageFormatProperties.maxExtent;
    return SVkFormatModifier{.props     = mod,
                             .maxExtent = {
                                 .width  = me.width,
                                 .height = me.height,
                             }};
}

CHyprVulkanDevice::~CHyprVulkanDevice() {
    if (m_device) {
        if (m_commandPool)
            vkDestroyCommandPool(vkDevice(), m_commandPool, nullptr);

        if (m_timelineSemaphore != VK_NULL_HANDLE)
            vkDestroySemaphore(m_device, m_timelineSemaphore, nullptr);

        vkDestroyDevice(m_device, nullptr);
    }

    if (m_drmFd >= 0)
        close(m_drmFd);
}
