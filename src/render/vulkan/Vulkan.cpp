#include "Vulkan.hpp"
#include "debug/log/Logger.hpp"
#include "render/vulkan/CommandBuffer.hpp"
#include "utils.hpp"

#include "../../macros.hpp"
#include <drm_fourcc.h>
#include <hyprutils/memory/SharedPtr.hpp>
#include <sys/stat.h>
#include <cstdint>
#include <string>
#include <sys/sysmacros.h>
#include <vector>
#include <vulkan/vulkan_core.h>

// DEBUG
#include "Compositor.hpp"

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

    VkDebugUtilsMessengerCreateInfoEXT debugInfo = {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &debugCallback,
        .pUserData       = this,
    };

    VkInstanceCreateInfo instanceInfo = {.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                         .pNext                   = hasDebug ? &debugInfo : nullptr,
                                         .pApplicationInfo        = &application_info,
                                         .enabledLayerCount       = 0,
                                         .ppEnabledLayerNames     = nullptr,
                                         .enabledExtensionCount   = enabledExtensions.size(),
                                         .ppEnabledExtensionNames = enabledExtensions.data()};

    if (vkCreateInstance(&instanceInfo, nullptr, &m_instance) != VK_SUCCESS) {
        CRIT("Could not create instance");
    }

    if (hasDebug) {
        Log::logger->log(Log::DEBUG, "activating vulkan debug");
        loadVulkanProc(&m_proc.vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessengerEXT");
        loadVulkanProc(&m_proc.vkDestroyDebugUtilsMessengerEXT, "vkDestroyDebugUtilsMessengerEXT");

        if (m_proc.vkCreateDebugUtilsMessengerEXT)
            m_proc.vkCreateDebugUtilsMessengerEXT(m_instance, &debugInfo, nullptr, &m_debug);
        else
            Log::logger->log(Log::ERR, "vkCreateDebugUtilsMessengerEXT not found");
    }

    // DEBUG
    m_device = getDevice(g_pCompositor->m_drmRenderNode.fd >= 0 ? g_pCompositor->m_drmRenderNode.fd : g_pCompositor->m_drm.fd);
    Log::logger->log(Log::DEBUG, "[VULKAN] Init {}", m_device && m_device->good());
    if (m_device->good())
        m_device->loadFormats();
}

CHyprVulkanImpl::~CHyprVulkanImpl() {
    Log::logger->log(Log::DEBUG, "Destroying vulkan renderer");

    if (m_debug != VK_NULL_HANDLE && m_proc.vkDestroyDebugUtilsMessengerEXT)
        m_proc.vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug, nullptr);

    if (m_instance != VK_NULL_HANDLE)
        vkDestroyInstance(m_instance, nullptr);
}

SP<CHyprVulkanDevice> CHyprVulkanImpl::getDevice(int drmFd) {
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
            return makeShared<CHyprVulkanDevice>(device, extensions);
        }
    }

    return nullptr;
}

SP<CHyprVkCommandBuffer> CHyprVulkanImpl::begin() {
    if (!m_commandBuffer)
        m_commandBuffer = makeShared<CHyprVkCommandBuffer>(m_device);

    m_device->m_timelinePoint++;
    m_commandBuffer->begin();
    return m_commandBuffer;
}

void CHyprVulkanImpl::end() {
    m_commandBuffer->end(m_device->m_timelinePoint);
}
