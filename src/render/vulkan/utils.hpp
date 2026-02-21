#pragma once

#include <aquamarine/buffer/Buffer.hpp>
#include <fcntl.h>
#include <hyprutils/math/Vector2D.hpp>
#include <sys/stat.h>
#include <vulkan/vulkan.h>
#include "../../debug/log/Logger.hpp"
#include "../../macros.hpp"
#include "render/vulkan/types.hpp"

#define CRIT(...)                                                                                                                                                                  \
    Log::logger->log(Log::CRIT, __VA_ARGS__);                                                                                                                                      \
    abort();

#if ISDEBUG

#include <vulkan/vk_enum_string_helper.h>
#define IF_VKFAIL(proc, ...) if (const auto [callResult, callName] = std::tuple{proc(__VA_ARGS__), #proc}; callResult != VK_SUCCESS)
#define LOG_VKFAIL           Log::logger->log(Log::ERR, "{} failed: {}", callName, string_VkResult(callResult))

#define SET_VK_NAME(target, name, type)                                                                                                                                            \
    {                                                                                                                                                                              \
        const std::string             tmp  = name;                                                                                                                                 \
        VkDebugUtilsObjectNameInfoEXT info = {                                                                                                                                     \
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,                                                                                                    \
            .objectType   = type,                                                                                                                                                  \
            .objectHandle = (uintptr_t)(target),                                                                                                                                   \
            .pObjectName  = tmp.c_str(),                                                                                                                                           \
        };                                                                                                                                                                         \
        if (g_pHyprVulkan->device()->m_proc.vkSetDebugUtilsObjectNameEXT)                                                                                                          \
            g_pHyprVulkan->device()->m_proc.vkSetDebugUtilsObjectNameEXT(g_pHyprVulkan->vkDevice(), &info);                                                                        \
    }

#define VK_LABEL_BEGIN(name)                                                                                                                                                       \
    {                                                                                                                                                                              \
        const std::string    tmp  = name;                                                                                                                                          \
        VkDebugUtilsLabelEXT info = {                                                                                                                                              \
            .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,                                                                                                                 \
            .pLabelName = tmp.c_str(),                                                                                                                                             \
        };                                                                                                                                                                         \
        if (g_pHyprVulkan->device()->m_proc.vkQueueBeginDebugUtilsLabelEXT)                                                                                                        \
            g_pHyprVulkan->device()->m_proc.vkQueueBeginDebugUtilsLabelEXT(g_pHyprVulkan->device()->queue(), &info);                                                               \
    }

#define VK_LABEL_END                                                                                                                                                               \
    {                                                                                                                                                                              \
        if (g_pHyprVulkan->device()->m_proc.vkQueueEndDebugUtilsLabelEXT)                                                                                                          \
            g_pHyprVulkan->device()->m_proc.vkQueueEndDebugUtilsLabelEXT(g_pHyprVulkan->device()->queue());                                                                        \
    }

#define VK_CB_LABEL_BEGIN(cb, name)                                                                                                                                                \
    {                                                                                                                                                                              \
        const std::string    tmp  = name;                                                                                                                                          \
        VkDebugUtilsLabelEXT info = {                                                                                                                                              \
            .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,                                                                                                                 \
            .pLabelName = tmp.c_str(),                                                                                                                                             \
        };                                                                                                                                                                         \
        if (g_pHyprVulkan->device()->m_proc.vkCmdBeginDebugUtilsLabelEXT)                                                                                                          \
            g_pHyprVulkan->device()->m_proc.vkCmdBeginDebugUtilsLabelEXT(cb, &info);                                                                                               \
    }

#define VK_CB_LABEL_END(cb)                                                                                                                                                        \
    {                                                                                                                                                                              \
        if (g_pHyprVulkan->device()->m_proc.vkCmdEndDebugUtilsLabelEXT)                                                                                                            \
            g_pHyprVulkan->device()->m_proc.vkCmdEndDebugUtilsLabelEXT(cb);                                                                                                        \
    }

#else
#define IF_VKFAIL(proc, ...)            if (proc(__VA_ARGS__) != VK_SUCCESS)
#define LOG_VKFAIL                      ;
#define SET_VK_NAME(target, name, type) ;
#define VK_LABEL_BEGIN(name)            ;
#define VK_LABEL_END                    ;
#define VK_CB_LABEL_BEGIN(cb, name)     ;
#define VK_CB_LABEL_END(cb)             ;
#endif

#define SET_VK_CB_NAME(target, name)       SET_VK_NAME(target, name, VK_OBJECT_TYPE_COMMAND_BUFFER);
#define SET_VK_IMG_NAME(target, name)      SET_VK_NAME(target, name, VK_OBJECT_TYPE_IMAGE);
#define SET_VK_IMG_VIEW_NAME(target, name) SET_VK_NAME(target, name, VK_OBJECT_TYPE_IMAGE_VIEW);
#define SET_VK_SHADER_NAME(target, name)   SET_VK_NAME(target, name, VK_OBJECT_TYPE_SHADER_MODULE);
#define SET_VK_PIPELINE_NAME(target, name) SET_VK_NAME(target, name, VK_OBJECT_TYPE_PIPELINE);

// VK_OBJECT_TYPE_PIPELINE_CACHE = 16,
// VK_OBJECT_TYPE_PIPELINE_LAYOUT = 17,
// VK_OBJECT_TYPE_RENDER_PASS = 18,
// VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT = 20,
// VK_OBJECT_TYPE_DESCRIPTOR_POOL = 22,
// VK_OBJECT_TYPE_DESCRIPTOR_SET = 23,
// VK_OBJECT_TYPE_FRAMEBUFFER = 24,

bool              isIgnoredDebugMessage(const std::string& idName);
bool              hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name);
std::string       resultToStr(VkResult res);
int               findVkMemType(VkPhysicalDevice dev, VkMemoryPropertyFlags flags, uint32_t req_bits);
bool              isDisjoint(const Aquamarine::SDMABUFAttrs& attrs);
void              startRenderPassHelper(VkRenderPass renderPass, VkFramebuffer fb, const Hyprutils::Math::Vector2D& size, VkCommandBuffer cb);
SVkVertShaderData matToVertShader(const std::array<float, 9> mat);
void              drawRegionRects(const CRegion& region, VkCommandBuffer cb);