#include "utils.hpp"

// "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed"
bool isIgnoredDebugMessage(const std::string& idName) {
    return false;
}

std::string resultToStr(VkResult res) {
    return std::to_string(res);
}

bool hasExtension(const std::vector<VkExtensionProperties>& extensions, const std::string& name) {
    return std::ranges::any_of(extensions, [name](const auto& ext) { return ext.extensionName == name; });
};
