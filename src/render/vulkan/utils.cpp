#include "utils.hpp"
#include "render/vulkan/Vulkan.hpp"

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

int findVkMemType(VkPhysicalDevice dev, VkMemoryPropertyFlags flags, uint32_t bits) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(dev, &props);

    for (unsigned i = 0; i < props.memoryTypeCount; ++i) {
        if (bits & (1 << i)) {
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

void startRenderPassHelper(VkRenderPass renderPass, VkFramebuffer fb, const Vector2D& size, VkCommandBuffer cb) {
    VkRenderPassBeginInfo info = {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = renderPass,
        .framebuffer = fb,
        .renderArea =
            {
                .extent = {size.x, size.y},
            },
        .clearValueCount = 0,
    };

    vkCmdBeginRenderPass(cb, &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {
        .width    = size.x,
        .height   = size.y,
        .maxDepth = 1,
    };
    vkCmdSetViewport(cb, 0, 1, &viewport);
}