#include "Vulkan.hpp"
#include "debug/log/Logger.hpp"
#include "render/Renderer.hpp"
#include "render/VKRenderer.hpp"
#include "render/vulkan/CommandBuffer.hpp"
#include "render/vulkan/DescriptorPool.hpp"
#include "render/vulkan/MemoryBuffer.hpp"
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

static const VkDeviceSize  MIN_SHARED_BUFFER_SIZE            = (VkDeviceSize)1024 * 1024;
static const VkDeviceSize  MAX_SHARED_BUFFER_SIZE            = MIN_SHARED_BUFFER_SIZE * 256;
static const int           MAX_SHARED_BUFFER_UNUSED_DURATION = 10000;
static const size_t        INITIAL_DESCRIPTOR_POOL_SIZE      = 250;
static const int           MAX_COMMAND_BUFFERS               = 64;

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
    uint32_t deviceCount;

    IF_VKFAIL(vkEnumeratePhysicalDevices, m_instance, &deviceCount, nullptr) {
        LOG_VKFAIL;
        return nullptr;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    IF_VKFAIL(vkEnumeratePhysicalDevices, m_instance, &deviceCount, devices.data()) {
        LOG_VKFAIL;
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
        auto     res      = vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
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

bool CHyprVulkanImpl::waitCommandBuffer(WP<CHyprVkCommandBuffer> cb) {
    ASSERT(cb && (cb->vk() != VK_NULL_HANDLE) && !cb->busy());

    const auto             semaphore = m_device->timelineSemaphore();
    VkSemaphoreWaitInfoKHR waitInfo  = {
         .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
         .semaphoreCount = 1,
         .pSemaphores    = &semaphore,
         .pValues        = &cb->m_timelinePoint,
    };
    IF_VKFAIL(vkWaitSemaphores, m_device->vkDevice(), &waitInfo, UINT64_MAX) {
        LOG_VKFAIL;
        return false;
    }

    return true;
}

SP<CHyprVkCommandBuffer> CHyprVulkanImpl::acquireCB() {
    SP<CHyprVkCommandBuffer> commandBuffer;
    SP<CHyprVkCommandBuffer> wait;
    for (const auto& cb : m_commandBuffers) {
        if (cb->busy())
            continue;

        if (cb->m_timelinePoint <= m_device->timelinePoint()) {
            commandBuffer = cb;
            break;
        }

        if (!wait || cb->m_timelinePoint < wait->m_timelinePoint)
            wait = cb;
    }

    if (!commandBuffer && m_commandBuffers.size() < MAX_COMMAND_BUFFERS) {
        commandBuffer = m_commandBuffers.emplace_back(makeShared<CHyprVkCommandBuffer>(m_device));
    } else if (wait && waitCommandBuffer(wait))
        commandBuffer = wait;

    commandBuffer->begin();
    commandBuffer->useFB(dc<CHyprVKRenderer*>(g_pHyprRenderer.get())->m_currentRenderbuffer);
    return commandBuffer;
}

SP<CHyprVkCommandBuffer> CHyprVulkanImpl::begin() {
    std::erase_if(m_sharedBuffers, [](const auto buf) { return buf->m_lastUsed.getMillis() > MAX_SHARED_BUFFER_UNUSED_DURATION; });

    // TODO cleanup command buffers

    m_currentRenderCB = acquireCB();
    return m_currentRenderCB;
}

SP<CHyprVkCommandBuffer> CHyprVulkanImpl::endStage() {
    const auto stage = stageCB().lock();
    m_currentStageCB.reset();

    m_device->m_timelinePoint++;
    stage->end(m_device->m_timelinePoint);
    return stage;
}

bool CHyprVulkanImpl::submitStage() {
    auto stage = endStage();
    if (!stage)
        return false;

    auto                             sem = m_device->timelineSemaphore();
    auto                             cb  = stage->vk();

    VkTimelineSemaphoreSubmitInfoKHR timelineSubmitInfo = {
        .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues    = &stage->m_timelinePoint,
    };
    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = &timelineSubmitInfo,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cb,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &sem,
    };
    IF_VKFAIL(vkQueueSubmit, m_device->queue(), 1, &submitInfo, VK_NULL_HANDLE) {
        LOG_VKFAIL;
        return false;
    }

    return waitCommandBuffer(stage);
}

void CHyprVulkanImpl::end() {
    auto                      stage = endStage();

    VkCommandBufferSubmitInfo stageInfo = {
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = stage->vk(),
    };
    VkSemaphoreSubmitInfo stageSignal = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m_device->timelineSemaphore(),
        .value     = stage->m_timelinePoint,
    };
    VkSubmitInfo2 stageSubmit = {
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &stageInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &stageSignal,
    };

    VkSemaphoreSubmitInfo stageWait;
    if (m_lastStagePoint > 0) {
        stageWait = {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m_device->timelineSemaphore(),
            .value     = m_lastStagePoint,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        };

        stageSubmit.waitSemaphoreInfoCount = 1;
        stageSubmit.pWaitSemaphoreInfos    = &stageWait;
    }

    m_lastStagePoint = stage->m_timelinePoint;

    m_device->m_timelinePoint++;
    m_currentRenderCB->end(m_device->m_timelinePoint);

    std::vector<VkSemaphoreSubmitInfo> waitSemaphores;
    std::vector<VkSemaphoreSubmitInfo> signalSemaphores;

    // TODO sync
    // waitSemaphores.push_back({
    //     .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    //     .semaphore = m_waitSemaphore,
    //     .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    // });

    // signalSemaphores.push_back({
    //     .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    //     .semaphore = m_signalSemaphore,
    //     .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    // });

    signalSemaphores.push_back({
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m_device->timelineSemaphore(),
        .value     = m_device->m_timelinePoint,
        // .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    });

    const std::array<VkCommandBufferSubmitInfo, 1> cmdBufferInfo{{{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = m_currentRenderCB->vk(),
    }}};

    const VkSubmitInfo2                            renderSubmit = {
                                   .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        // .waitSemaphoreInfoCount   = waitSemaphores.size(),
        // .pWaitSemaphoreInfos      = waitSemaphores.data(),
                                   .commandBufferInfoCount   = cmdBufferInfo.size(),
                                   .pCommandBufferInfos      = cmdBufferInfo.data(),
                                   .signalSemaphoreInfoCount = signalSemaphores.size(),
                                   .pSignalSemaphoreInfos    = signalSemaphores.data(),
    };
    const std::array<VkSubmitInfo2, 2> submitInfo = {stageSubmit, renderSubmit};

    if (vkQueueSubmit2(m_device->queue(), uint32_t(submitInfo.size()), submitInfo.data(), VK_NULL_HANDLE) != VK_SUCCESS) {
        CRIT("vkQueueSubmit2 failed");
    }
}

SP<CHyprVulkanDevice> CHyprVulkanImpl::device() {
    return m_device;
}

VkDevice CHyprVulkanImpl::vkDevice() {
    return m_device->vkDevice();
}

WP<CHyprVkCommandBuffer> CHyprVulkanImpl::renderCB() {
    return m_currentRenderCB;
}

WP<CHyprVkCommandBuffer> CHyprVulkanImpl::stageCB() {
    if (!m_currentStageCB)
        m_currentStageCB = acquireCB();

    return m_currentStageCB;
};

WP<CVKMemorySpan> CHyprVulkanImpl::getMemorySpan(VkDeviceSize size, VkDeviceSize alignment) {
    for (const auto& buf : m_sharedBuffers) {
        auto offset = buf->offset();
        offset += alignment - 1 - ((offset + alignment - 1) % alignment);
        if (buf->available() >= offset + size)
            return buf->allocate(size, offset);
    }

    const auto bufSize = std::max(size * 2, MIN_SHARED_BUFFER_SIZE);
    if (bufSize > MAX_SHARED_BUFFER_SIZE) {
        Log::logger->log(Log::ERR, "Requested buffer size it too big {}({}) > {}", bufSize, size, MAX_SHARED_BUFFER_SIZE);
        return SP<CVKMemorySpan>(nullptr);
    }

    const auto buffer = m_sharedBuffers.emplace_back(CVKMemoryBuffer::create(m_device, bufSize));
    return buffer->allocate(size);
}

SP<CVKDescriptorPool> CHyprVulkanImpl::allocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet* ds) {
    for (const auto& pool : m_dsPools) {
        const auto res = pool->allocateSet(layout, ds);
        switch (res) {
            case VK_ERROR_FRAGMENTED_POOL:
            case VK_ERROR_OUT_OF_POOL_MEMORY: continue;
            case VK_SUCCESS: return pool;
            default: Log::logger->log(Log::ERR, "failed to allocate descriptor set on existing pool: {}", (unsigned)res); return nullptr;
        }
    }

    const auto size = m_lastDsPoolSize ? m_lastDsPoolSize * 2 : INITIAL_DESCRIPTOR_POOL_SIZE;
    auto       pool = m_dsPools.emplace_back(makeShared<CVKDescriptorPool>(m_device, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, size));
    const auto res  = pool->allocateSet(layout, ds);

    if (res == VK_SUCCESS)
        return pool;

    Log::logger->log(Log::ERR, "failed to allocate descriptor set on a new pool: {}", (unsigned)res);
    return nullptr;
}
