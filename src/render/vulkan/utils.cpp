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

int findVkMemType(VkPhysicalDevice dev, VkMemoryPropertyFlags flags, uint32_t req_bits) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(dev, &props);

    for (unsigned i = 0; i < props.memoryTypeCount; ++i) {
        if (req_bits & (1 << i)) {
            if ((props.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        }
    }

    return -1;
}

bool isDisjoint(const Aquamarine::SDMABUFAttrs& attrs) {
    if (attrs.planes == 1) {
        return false;
    }

    struct stat fdStat;
    if (fstat(attrs.fds[0], &fdStat) != 0)
        return true;

    for (int i = 1; i < attrs.planes; i++) {
        struct stat fdStat2;
        if (fstat(attrs.fds[i], &fdStat2) != 0)
            return true;

        if (fdStat.st_ino != fdStat2.st_ino) {
            return true;
        }
    }

    return false;
}
