#pragma once

#include <vulkan/vulkan.h>
#include "../../helpers/Format.hpp"

struct SVkFormatModifier {
    VkDrmFormatModifierPropertiesEXT props;
    VkExtent2D                       maxExtent;
    bool                             canSrgb = false;
};

struct SVkFormatProps {
    SPixelFormat format;

    struct {
        VkExtent2D           maxExtent;
        VkFormatFeatureFlags features;
        bool                 canSrgb = false;
    } shm;

    struct {
        std::vector<SVkFormatModifier> renderModifiers;
        std::vector<SVkFormatModifier> textureModifiers;
    } dmabuf;

    bool hasSrgb() const {
        return format.vkSrgbFormat != VK_FORMAT_UNDEFINED;
    }
};

static const VkFormatFeatureFlags SHM_TEX_FEATURES =
    VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
static const VkFormatFeatureFlags DMA_TEX_FEATURES = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
static const VkFormatFeatureFlags YCC_TEX_FEATURES = VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT | VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT;

static const VkImageUsageFlags    VULKAN_SHM_TEX_USAGE = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
static const VkImageUsageFlags    VULKAN_DMA_TEX_USAGE = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

static const VkFormatFeatureFlags RENDER_FEATURES = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;

struct SVkVertShaderData {
    float mat4[4][4];
    float uvOffset[2];
    float uvSize[2];
};

struct SVkFragShaderData {
    float matrix[4][4];
    float alpha;
    float luminance_multiplier;
};
