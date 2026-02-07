#pragma once

#include <vulkan/vulkan.h>
#include "../../debug/log/Logger.hpp"

#define CRIT(...)                                                                                                                                                                  \
    Log::logger->log(Log::CRIT, __VA_ARGS__);                                                                                                                                      \
    return; // abort();

bool        isIgnoredDebugMessage(const std::string& idName);
bool        hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name);
std::string resultToStr(VkResult res);
