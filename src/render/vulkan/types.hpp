#pragma once

#include <cmath>
#include <vulkan/vulkan.h>
#include "../../helpers/Format.hpp"

namespace Render::VK {

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

    struct alignas(16) SShaderTargetPrimaries {
        float xyz[3][3];
        float sdrSaturation;
        float sdrBrightnessMultiplier;
    };

    struct alignas(16) SShaderTonemap {
        float maxLuminance;
        float dstMaxLuminance;
        float dstRefLuminance;
    };

    struct alignas(16) SShaderCM {
        int   sourceTF; // eTransferFunction
        int   targetTF; // eTransferFunction
        float srcRefLuminance;

        float srcTFRange[2];
        float dstTFRange[2];

        float convertMatrix[3][3];
    };

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

    struct SVkFragShaderData {
        float alpha             = 1.0f;
        float tint              = 1.0f;
        int   discardMode       = 0;
        float discardAlphaValue = 0.0f;
    };

    static_assert(sizeof(SVkVertShaderData) + sizeof(SVkFragShaderData) + sizeof(SShaderTargetPrimaries) + sizeof(SShaderTonemap) + sizeof(SShaderCM) + sizeof(SRounding) <= 256,
                  "Main texture shader push constants are too large");

    struct alignas(16) SVkBorderShaderData {
        float fullSizeUntransformed[2];
        float radiusOuter;
        float thick;
        float angle;
        float angle2;
        float alpha;
    };

    struct alignas(16) SVkBorderGradientShaderData {
        float gradient[10][4];
        float gradient2[10][4];
        int   gradientLength;
        int   gradient2Length;
        float gradientLerp;
    };

    struct alignas(16) SVkRectShaderData {
        float color[4];
    };

    struct alignas(16) SVkShadowShaderData {
        float color[4];
        float bottomRight[2];
        float range;
        float shadowPower;
    };

    struct alignas(16) SVkPrepareShaderData {
        float contrast;
        float brightness;
        float sdrBrightnessMultiplier = 1.0;
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
}