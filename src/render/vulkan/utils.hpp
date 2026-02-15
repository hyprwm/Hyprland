#pragma once

#include <aquamarine/buffer/Buffer.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <vulkan/vulkan.h>
#include "../../debug/log/Logger.hpp"

#define CRIT(...)                                                                                                                                                                  \
    Log::logger->log(Log::CRIT, __VA_ARGS__);                                                                                                                                      \
    abort();

bool        isIgnoredDebugMessage(const std::string& idName);
bool        hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name);
std::string resultToStr(VkResult res);
int         findVkMemType(VkPhysicalDevice dev, VkMemoryPropertyFlags flags, uint32_t req_bits);
bool        isDisjoint(const Aquamarine::SDMABUFAttrs& attrs);