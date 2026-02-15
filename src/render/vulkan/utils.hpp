#pragma once

#include <aquamarine/buffer/Buffer.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <vulkan/vulkan.h>
#include "../../debug/log/Logger.hpp"

#define CRIT(...)                                                                                                                                                                  \
    Log::logger->log(Log::CRIT, __VA_ARGS__);                                                                                                                                      \
    abort();

#define IF_VKFAIL(proc, ...) if (const auto [callResult, callName] = std::tuple{proc(__VA_ARGS__), #proc}; callResult != VK_SUCCESS)

#define LOG_VKFAIL Log::logger->log(Log::ERR, "{} failed: {}", callName, (uint64_t)callResult)

bool        isIgnoredDebugMessage(const std::string& idName);
bool        hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name);
std::string resultToStr(VkResult res);
int         findVkMemType(VkPhysicalDevice dev, VkMemoryPropertyFlags flags, uint32_t req_bits);
bool        isDisjoint(const Aquamarine::SDMABUFAttrs& attrs);