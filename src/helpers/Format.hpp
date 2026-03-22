#pragma once

#include <cstdint>
#include <drm_fourcc.h>
#include <string>
#include <GLES3/gl32.h>
#include <aquamarine/backend/Misc.hpp>
#include <map>
#include <vulkan/vulkan_core.h>

using DRMFormat = uint32_t;
using SHMFormat = uint32_t;

using SDRMFormat = Aquamarine::SDRMFormat;

namespace NFormatUtils {
    SHMFormat   drmToShm(DRMFormat drm);
    DRMFormat   shmToDRM(SHMFormat shm);
    bool        isFormatYUV(uint32_t drmFormat);
    std::string drmFormatName(DRMFormat drm);
    std::string drmModifierName(uint64_t mod);
    DRMFormat   alphaFormat(DRMFormat prevFormat);
};

// TODO move to hyprgraphics
namespace Render::VK {
    struct SVkFormatInfo {
        DRMFormat drmFormat    = DRM_FORMAT_INVALID;
        VkFormat  vkFormat     = VK_FORMAT_UNDEFINED;
        VkFormat  vkSrgbFormat = VK_FORMAT_UNDEFINED;
        bool      isYCC        = false;
    };

    extern const std::map<DRMFormat, SVkFormatInfo> FORMATS;
}
