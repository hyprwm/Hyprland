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
#else
#define IF_VKFAIL(proc, ...) if (proc(__VA_ARGS__) != VK_SUCCESS)
#define LOG_VKFAIL           ;
#endif

bool        isIgnoredDebugMessage(const std::string& idName);
bool        hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name);
std::string resultToStr(VkResult res);
int         findVkMemType(VkPhysicalDevice dev, VkMemoryPropertyFlags flags, uint32_t req_bits);
bool        isDisjoint(const Aquamarine::SDMABUFAttrs& attrs);