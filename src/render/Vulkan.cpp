#include "Vulkan.hpp"

#include "../debug/log/Logger.hpp"
#include "macros.hpp"
#include <algorithm>
#include <drm_fourcc.h>
#include <sys/stat.h>
#include <cstdint>
#include <string>
#include <sys/sysmacros.h>
#include <vector>
#include <vulkan/vulkan_core.h>

// DEBUG
#include "Compositor.hpp"

#define CRIT(...)                                                                                                                                                                  \
    Log::logger->log(Log::CRIT, __VA_ARGS__);                                                                                                                                      \
    return; // abort();

static const VkFormatFeatureFlags SHM_TEX_FEATURES =
    VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
static const VkFormatFeatureFlags DMA_TEX_FEATURES = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
static const VkFormatFeatureFlags YCC_TEX_FEATURES = VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT | VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT;

static const VkImageUsageFlags    VULKAN_SHM_TEX_USAGE = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
static const VkImageUsageFlags    VULKAN_DMA_TEX_USAGE = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

static const VkFormatFeatureFlags RENDER_FEATURES = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;

// "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed"
static bool isIgnoredDebugMessage(const std::string& idName) {
    return false;
}

static std::string resultToStr(VkResult res) {
    return std::to_string(res);
}

static bool hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name) {
    return std::ranges::any_of(extensions, [name](const auto& ext) { return ext.extensionName == name; });
};

static VKAPI_ATTR VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
                                         const VkDebugUtilsMessengerCallbackDataEXT* debugData, void* data) {

    if (debugData->pMessageIdName && isIgnoredDebugMessage(debugData->pMessageIdName))
        return false;

    Hyprutils::CLI::eLogLevel level;
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: level = Log::ERR; break;
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: level = Log::INFO; break;
    }

    Log::logger->log(level, "{} ({})", debugData->pMessage, debugData->pMessageIdName);
    if (debugData->queueLabelCount > 0) {
        const char* name = debugData->pQueueLabels[0].pLabelName;
        if (name)
            Log::logger->log(level, "    last label '{}'", name);
    }

    for (uint32_t i = 0; i < debugData->objectCount; ++i) {
        if (debugData->pObjects[i].pObjectName)
            Log::logger->log(level, "    involving '{}'", debugData->pMessage);
    }

    return false;
}

static void logDeviceInfo(const VkPhysicalDeviceProperties& props) {
    std::string type = "unknown";
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: type = "integrated"; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: type = "discrete"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: type = "cpu"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: type = "vgpu"; break;
        default: break;
    }

    Log::logger->log(Log::INFO, "Vulkan device: '{}'", props.deviceName);
    Log::logger->log(Log::INFO, "  Device type: '{}'", type);
    Log::logger->log(Log::INFO, "  Supported API version: {}.{}.{}", VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
    Log::logger->log(Log::INFO, "  Driver version: {}.{}.{}", VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion));
}

inline void CHyprVulkanImpl::loadVulkanProc(void* pProc, const char* name) {
    void* proc = rc<void*>(vkGetInstanceProcAddr(m_instance, name));
    if (proc == nullptr) {
        CRIT("[VULKAN] vkGetInstanceProcAddr({}) failed", name);
    }
    *sc<void**>(pProc) = proc;
}

CHyprVulkanImpl::CHyprVulkanImpl() {
    Log::logger->log(Log::DEBUG, "Creating vulkan renderer");

    loadVulkanProc(&m_proc.pfEnumInstanceVersion, "vkEnumerateInstanceVersion");

    uint32_t version;
    if (m_proc.pfEnumInstanceVersion(&version) != VK_SUCCESS || version < VK_API_VERSION_1_3) {
        CRIT("[VULKAN] Version is {}, required is {}", version, VK_API_VERSION_1_3);
    }

    uint32_t extensionCount = 0;
    auto     res            = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (res != VK_SUCCESS || extensionCount == 0) {
        CRIT("[VULKAN] Could not get extention count", version, VK_API_VERSION_1_3);
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) != VK_SUCCESS) {
        CRIT("[VULKAN] Could not get extentions");
    }

    for (const auto& ext : extensions) {
        Log::logger->log(Log::DEBUG, "[VULKAN] Extension {} {}", ext.extensionName, ext.specVersion);
    }

    std::vector<char*> enabledExtensions;

    const bool         hasDebug = ISDEBUG && hasExtension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (hasDebug)
        enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkApplicationInfo application_info = {
        .sType         = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName   = "hyprland",
        .engineVersion = HYPRLAND_VERSION_MAJOR * 10000 + HYPRLAND_VERSION_MINOR * 100 + HYPRLAND_VERSION_PATCH,
        .apiVersion    = VK_API_VERSION_1_3,
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_info = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &debugCallback,
        .pUserData       = this,
    };

    VkInstanceCreateInfo instance_info = {.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                          .pNext                   = hasDebug ? &debug_info : nullptr,
                                          .pApplicationInfo        = &application_info,
                                          .enabledLayerCount       = 0,
                                          .ppEnabledLayerNames     = nullptr,
                                          .enabledExtensionCount   = enabledExtensions.size(),
                                          .ppEnabledExtensionNames = enabledExtensions.data()};

    if (vkCreateInstance(&instance_info, nullptr, &m_instance) != VK_SUCCESS) {
        CRIT("Could not create instance");
    }

    if (hasDebug) {
        loadVulkanProc(&m_proc.vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessengerEXT");
        loadVulkanProc(&m_proc.vkDestroyDebugUtilsMessengerEXT, "vkDestroyDebugUtilsMessengerEXT");

        if (m_proc.vkCreateDebugUtilsMessengerEXT)
            m_proc.vkCreateDebugUtilsMessengerEXT(m_instance, &debug_info, nullptr, &m_debug);
        else
            Log::logger->log(Log::ERR, "vkCreateDebugUtilsMessengerEXT not found");
    }

    // DEBUG
    m_device = getDevice(g_pCompositor->m_drmRenderNode.fd >= 0 ? g_pCompositor->m_drmRenderNode.fd : g_pCompositor->m_drm.fd);
    Log::logger->log(Log::DEBUG, "[VULKAN] Init {}", m_device && m_device->good());
}

CHyprVulkanImpl::~CHyprVulkanImpl() {
    Log::logger->log(Log::DEBUG, "Destroying vulkan renderer");

    if (m_debug != VK_NULL_HANDLE && m_proc.vkDestroyDebugUtilsMessengerEXT)
        m_proc.vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug, nullptr);

    if (m_instance != VK_NULL_HANDLE)
        vkDestroyInstance(m_instance, nullptr);
}

UP<CHyprVulkanDevice> CHyprVulkanImpl::getDevice(int drmFd) {
    VkResult res;
    uint32_t deviceCount;

    res = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (res != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "Could not get physical device count: {}", resultToStr(res));
        return nullptr;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    res = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    if (res != VK_SUCCESS) {
        Log::logger->log(Log::ERR, "Could not retrieve physical devices: {}", resultToStr(res));
        return nullptr;
    }

    struct stat drmStat = {.st_dev = 0};
    if (drmFd >= 0 && fstat(drmFd, &drmStat) != 0) {
        Log::logger->log(Log::ERR, "fstat failed");
        return nullptr;
    }

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties devProps;
        vkGetPhysicalDeviceProperties(device, &devProps);

        logDeviceInfo(devProps);

        if (devProps.apiVersion < VK_API_VERSION_1_3)
            continue;

        uint32_t extCount = 0;
        res               = vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
        if (res != VK_SUCCESS || !extCount) {
            Log::logger->log(Log::ERR, "  Could not enumerate device extensions: {}", resultToStr(res));
            continue;
        }

        std::vector<VkExtensionProperties> extensions(extCount);
        res = vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, extensions.data());
        if (res != VK_SUCCESS) {
            Log::logger->log(Log::ERR, "  Could not enumerate device extensions", resultToStr(res));
            continue;
        }

        const bool                       hasDrmProps    = hasExtension(extensions, VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME);
        const bool                       hasDriverProps = hasExtension(extensions, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);

        VkPhysicalDeviceProperties2      props    = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        VkPhysicalDeviceDrmPropertiesEXT drmProps = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT};
        if (hasDrmProps) {
            drmProps.pNext = props.pNext;
            props.pNext    = &drmProps;
        }

        VkPhysicalDeviceDriverPropertiesKHR driverProps = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
        };
        if (hasDriverProps) {
            driverProps.pNext = props.pNext;
            props.pNext       = &driverProps;
        }

        vkGetPhysicalDeviceProperties2(device, &props);

        if (hasDriverProps)
            Log::logger->log(Log::INFO, "  Driver name: {} ({})", driverProps.driverName, driverProps.driverInfo);

        bool found;
        if (drmFd >= 0) {
            if (!hasDrmProps) {
                Log::logger->log(Log::DEBUG, "  Ignoring physical device \"{}\": VK_EXT_physical_device_drm not supported", devProps.deviceName);
                continue;
            }

            const auto primaryId = makedev(drmProps.primaryMajor, drmProps.primaryMinor);
            const auto renderId  = makedev(drmProps.renderMajor, drmProps.renderMinor);
            found                = primaryId == drmStat.st_rdev || renderId == drmStat.st_rdev;
        } else
            found = devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;

        if (found) {
            Log::logger->log(Log::INFO, "Found matching Vulkan physical device: {}", devProps.deviceName);
            return makeUnique<CHyprVulkanDevice>(device, extensions);
        }
    }

    return nullptr;
}

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

    const bool hasGlobalPriority = hasExtension(m_extensions, VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME);
    if (hasGlobalPriority) {
        // If global priorities are supported, request a high-priority context
        VkDeviceQueueGlobalPriorityCreateInfoKHR globalPriority = (VkDeviceQueueGlobalPriorityCreateInfoKHR){
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
    loadVulkanProc(&m_proc.vkQueueSubmit2KHR, "vkQueueSubmit2KHR");

    if (m_canExplicitSync) {
        loadVulkanProc(&m_proc.vkGetSemaphoreFdKHR, "vkGetSemaphoreFdKHR");
        loadVulkanProc(&m_proc.vkImportSemaphoreFdKHR, "vkImportSemaphoreFdKHR");
    }

    m_good = true;
}

bool CHyprVulkanDevice::good() {
    return m_good;
}

void CHyprVulkanDevice::loadFormats() {}

bool CHyprVulkanDevice::getFormat(const SPixelFormat& format) {
    if (format.isYCC && !m_canConvertYCC)
        return false;

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
        Log::logger->log(Log::ERR, "{} missing required features", NFormatUtils::drmFormatName(format.drmFormat));

    if (mods.drmFormatModifierCount > 0)
        isValid |= getModifiers(props, mods.drmFormatModifierCount);

    if (isValid)
        m_formats.emplace_back(props);

    return isValid;
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
            Log::logger->log(Log::ERR, "{} missing required render features", NFormatUtils::drmFormatName(props.format.drmFormat));

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
            Log::logger->log(Log::ERR, "{} missing required texture features", NFormatUtils::drmFormatName(props.format.drmFormat));
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
    if (m_device)
        vkDestroyDevice(m_device, nullptr);

    if (m_drmFd >= 0)
        close(m_drmFd);
}
