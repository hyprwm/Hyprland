#pragma once

#include <aquamarine/buffer/Buffer.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <vulkan/vulkan.h>
#include "../../debug/log/Logger.hpp"
#include "../../macros.hpp"

#define CRIT(...)                                                                                                                                                                  \
    Log::logger->log(Log::CRIT, __VA_ARGS__);                                                                                                                                      \
    abort();

#if ISDEBUG

#include <vulkan/vk_enum_string_helper.h>
#define IF_VKFAIL(proc, ...) if (const auto [callResult, callName] = std::tuple{proc(__VA_ARGS__), #proc}; callResult != VK_SUCCESS)
#define LOG_VKFAIL           Log::logger->log(Log::ERR, "{} failed: {}", callName, string_VkResult(callResult))

#define SET_VK_NAME(target, name, type)                                                                                                                                            \
    {                                                                                                                                                                              \
        VkDebugUtilsObjectNameInfoEXT info = {                                                                                                                                     \
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,                                                                                                    \
            .objectType   = type,                                                                                                                                                  \
            .objectHandle = (uintptr_t)(target),                                                                                                                                   \
            .pObjectName  = name,                                                                                                                                                  \
        };                                                                                                                                                                         \
        if (g_pHyprVulkan->device()->m_proc.vkSetDebugUtilsObjectNameEXT)                                                                                                          \
            g_pHyprVulkan->device()->m_proc.vkSetDebugUtilsObjectNameEXT(g_pHyprVulkan->vkDevice(), &info);                                                                        \
    }

#else
#define IF_VKFAIL(proc, ...)            if (proc(__VA_ARGS__) != VK_SUCCESS)
#define LOG_VKFAIL                      ;
#define SET_VK_NAME(target, name, type) ;
#endif

#define SET_VK_CB_NAME(target, name)       SET_VK_NAME(target, name, VK_OBJECT_TYPE_COMMAND_BUFFER);
#define SET_VK_IMG_NAME(target, name)      SET_VK_NAME(target, name, VK_OBJECT_TYPE_IMAGE);
#define SET_VK_IMG_VIEW_NAME(target, name) SET_VK_NAME(target, name, VK_OBJECT_TYPE_IMAGE_VIEW);

// VK_OBJECT_TYPE_SHADER_MODULE = 15,
// VK_OBJECT_TYPE_PIPELINE_CACHE = 16,
// VK_OBJECT_TYPE_PIPELINE_LAYOUT = 17,
// VK_OBJECT_TYPE_RENDER_PASS = 18,
// VK_OBJECT_TYPE_PIPELINE = 19,
// VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT = 20,
// VK_OBJECT_TYPE_DESCRIPTOR_POOL = 22,
// VK_OBJECT_TYPE_DESCRIPTOR_SET = 23,
// VK_OBJECT_TYPE_FRAMEBUFFER = 24,

bool        isIgnoredDebugMessage(const std::string& idName);
bool        hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name);
std::string resultToStr(VkResult res);
int         findVkMemType(VkPhysicalDevice dev, VkMemoryPropertyFlags flags, uint32_t req_bits);
bool        isDisjoint(const Aquamarine::SDMABUFAttrs& attrs);