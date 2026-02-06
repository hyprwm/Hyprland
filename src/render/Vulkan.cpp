#include "Vulkan.hpp"

#include "../debug/log/Logger.hpp"
#include "macros.hpp"
#include <cstdint>
#include <string>

#define CRIT(...)                                                                                                                                                                  \
    Log::logger->log(Log::CRIT, __VA_ARGS__);                                                                                                                                      \
    return; // abort();

static bool isIgnoredDebugMessage(const std::string& idName) {
    // we ignore some of the non-helpful warnings
    // static const char* const ignored[] = {
    //     // notifies us that shader output is not consumed since
    //     // we use the shared vertex buffer with uv output
    //     "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed",
    // };
    return false;
}

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
    if (m_proc.pfEnumInstanceVersion(&version) != VK_SUCCESS || version < VK_API_VERSION_1_1) {
        CRIT("[VULKAN] Version is {}, required is {}", version, VK_API_VERSION_1_1);
    }

    uint32_t extensionCount = 0;
    auto     res            = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (res != VK_SUCCESS || extensionCount == 0) {
        CRIT("[VULKAN] Could not get extention count", version, VK_API_VERSION_1_1);
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) != VK_SUCCESS) {
        CRIT("[VULKAN] Could not get extentions");
    }

    for (const auto& ext : extensions) {
        Log::logger->log(Log::DEBUG, "[VULKAN] Extension {} {}", ext.extensionName, ext.specVersion);
    }

    std::vector<char*> enabledExtensions;

    static auto hasExtension = [extensions](const std::string& name) { return std::ranges::any_of(extensions, [name](const auto& ext) { return ext.extensionName == name; }); };
    const bool  hasDebug     = ISDEBUG && hasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (hasDebug)
        enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkApplicationInfo application_info = {
        .sType         = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName   = "hyprland",
        .engineVersion = HYPRLAND_VERSION_MAJOR * 10000 + HYPRLAND_VERSION_MINOR * 100 + HYPRLAND_VERSION_PATCH,
        .apiVersion    = VK_API_VERSION_1_1,
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
}

CHyprVulkanImpl::~CHyprVulkanImpl() {
    Log::logger->log(Log::DEBUG, "Destroying vulkan renderer");

    if (m_debug != VK_NULL_HANDLE && m_proc.vkDestroyDebugUtilsMessengerEXT)
        m_proc.vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug, nullptr);

    if (m_instance != VK_NULL_HANDLE)
        vkDestroyInstance(m_instance, nullptr);
}
