#pragma once

#include <cmath>
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

    uint32_t bytesPerBlock() const {
        return format.bytesPerBlock > 0 ? format.bytesPerBlock : 4;
    }

    uint32_t pixelsPerBlock() const {
        const int32_t pixels = format.blockSize.size();
        return std::max(pixels, 1);
    }

    int32_t minStride(int32_t width) const {
        const int32_t pixels = pixelsPerBlock();
        const int32_t bytes  = bytesPerBlock();
        if (width > INT32_MAX / bytes)
            return 0;

        return std::ceil(width * bytes / pixels);
    }
};

static const VkFormatFeatureFlags SHM_TEX_FEATURES =
    VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
static const VkFormatFeatureFlags DMA_TEX_FEATURES = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
static const VkFormatFeatureFlags YCC_TEX_FEATURES = VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT | VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT;

static const VkImageUsageFlags    VULKAN_SHM_TEX_USAGE = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
static const VkImageUsageFlags    VULKAN_DMA_TEX_USAGE = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

static const VkFormatFeatureFlags RENDER_FEATURES = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;

struct alignas(16) SRounding {
    float radius;
    float power;
    float topLeft[2];
    float fullSize[2];
};

struct alignas(16) SVkVertShaderData {
    float mat4[4][4];
    float uvOffset[2];
    float uvSize[2];
};

struct alignas(16) SVkFragShaderData {
    float matrix[4][4];
    float alpha;
    float luminanceMultiplier;
};

struct alignas(16) SVkBorderShaderData {
    float fullSizeUntransformed[2];
    float radiusOuter;
    float thick;
    // float gradient[10][4];
    // float gradient2[10][4];
    // float     gradient[1][4];
    // float     gradient2[1][4];
    // int       gradientLength;
    // int       gradient2Length;
    float angle;
    float angle2;
    // float     gradientLerp;
    float     alpha;
    SRounding rounding;
};

struct alignas(16) SVkBorderGradientShaderData {
    float gradient[10][4];
    float gradient2[10][4];
    int   gradientLength;
    int   gradient2Length;
    float gradientLerp;
};

struct alignas(16) SVkRectShaderData {
    float     color[4];
    SRounding rounding;
};

struct alignas(16) SVkShadowShaderData {
    float     color[4];
    float     bottomRight[2];
    float     range;
    float     shadowPower;
    SRounding rounding;
};

struct alignas(16) SVkPrepareShaderData {
    float contrast;
    float brightness;
};

struct alignas(16) SVkBlur1ShaderData {
    float radius;
    float halfpixel[2];
    int   passes;
    float vibrancy;
    float vibrancyDarkness;
};

struct alignas(16) SVkBlur2ShaderData {
    float radius;
    float halfpixel[2];
};

struct alignas(16) SVkFinishShaderData {
    float noise;
    float brightness;
};