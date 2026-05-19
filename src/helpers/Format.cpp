#include "Format.hpp"
#include <wayland-server-protocol.h>
#include <xf86drm.h>
#include <drm_fourcc.h>

const std::map<DRMFormat, Render::VK::SVkFormatInfo> Render::VK::FORMATS = {
    {
        DRM_FORMAT_ARGB8888,
        {
            .drmFormat = DRM_FORMAT_ARGB8888,
            .vkFormat  = VK_FORMAT_B8G8R8A8_UNORM,
        },
    },
    {DRM_FORMAT_XRGB8888,
     {
         .drmFormat    = DRM_FORMAT_XRGB8888,
         .vkFormat     = VK_FORMAT_B8G8R8A8_UNORM,
         .vkSrgbFormat = VK_FORMAT_B8G8R8A8_SRGB,

     }},
    {DRM_FORMAT_XBGR8888,
     {
         .drmFormat    = DRM_FORMAT_XBGR8888,
         .vkFormat     = VK_FORMAT_R8G8B8A8_UNORM,
         .vkSrgbFormat = VK_FORMAT_R8G8B8A8_SRGB,

     }},
    {DRM_FORMAT_ABGR8888,
     {
         .drmFormat = DRM_FORMAT_ABGR8888,
         .vkFormat  = VK_FORMAT_R8G8B8A8_UNORM,

     }},
    {DRM_FORMAT_BGR888,
     {
         .drmFormat    = DRM_FORMAT_BGR888,
         .vkFormat     = VK_FORMAT_R8G8B8_UNORM,
         .vkSrgbFormat = VK_FORMAT_R8G8B8_SRGB,

     }},
    {DRM_FORMAT_RGBX4444,
     {
         .drmFormat = DRM_FORMAT_RGBX4444,
         .vkFormat  = VK_FORMAT_R4G4B4A4_UNORM_PACK16,

     }},
    {DRM_FORMAT_RGBA4444,
     {
         .drmFormat = DRM_FORMAT_RGBA4444,
         .vkFormat  = VK_FORMAT_R4G4B4A4_UNORM_PACK16,

     }},
    {DRM_FORMAT_RGBX5551,
     {
         .drmFormat = DRM_FORMAT_RGBX5551,
         .vkFormat  = VK_FORMAT_R5G5B5A1_UNORM_PACK16,

     }},
    {DRM_FORMAT_RGBA5551,
     {
         .drmFormat = DRM_FORMAT_RGBA5551,
         .vkFormat  = VK_FORMAT_R5G5B5A1_UNORM_PACK16,

     }},
    {DRM_FORMAT_RGB565,
     {
         .drmFormat = DRM_FORMAT_RGB565,
         .vkFormat  = VK_FORMAT_R5G6B5_UNORM_PACK16,

     }},
    {DRM_FORMAT_XBGR2101010,
     {
         .drmFormat = DRM_FORMAT_XBGR2101010,
         .vkFormat  = VK_FORMAT_A2B10G10R10_UNORM_PACK32,

     }},
    {DRM_FORMAT_ABGR2101010,
     {
         .drmFormat = DRM_FORMAT_ABGR2101010,
         .vkFormat  = VK_FORMAT_A2B10G10R10_UNORM_PACK32,

     }},
    {DRM_FORMAT_XRGB2101010,
     {
         .drmFormat = DRM_FORMAT_XRGB2101010,
         .vkFormat  = VK_FORMAT_A2R10G10B10_UNORM_PACK32,

     }},
    {DRM_FORMAT_ARGB2101010,
     {
         .drmFormat = DRM_FORMAT_ARGB2101010,
         .vkFormat  = VK_FORMAT_A2R10G10B10_UNORM_PACK32,

     }},
    {DRM_FORMAT_XBGR16161616F,
     {
         .drmFormat = DRM_FORMAT_XBGR16161616F,
         .vkFormat  = VK_FORMAT_R16G16B16A16_SFLOAT,

     }},
    {DRM_FORMAT_ABGR16161616F,
     {
         .drmFormat = DRM_FORMAT_ABGR16161616F,
         .vkFormat  = VK_FORMAT_R16G16B16A16_SFLOAT,

     }},
    {DRM_FORMAT_XBGR16161616,
     {
         .drmFormat = DRM_FORMAT_XBGR16161616,
         .vkFormat  = VK_FORMAT_R16G16B16A16_UNORM,

     }},
    {DRM_FORMAT_ABGR16161616,
     {
         .drmFormat = DRM_FORMAT_ABGR16161616,
         .vkFormat  = VK_FORMAT_R16G16B16A16_UNORM,

     }},
    {DRM_FORMAT_YVYU,
     {
         .drmFormat = DRM_FORMAT_YVYU,
         .isYCC     = true,

     }},
    {DRM_FORMAT_VYUY,
     {
         .drmFormat = DRM_FORMAT_VYUY,
         .isYCC     = true,

     }},
    {DRM_FORMAT_R8,
     {
         .drmFormat    = DRM_FORMAT_R8,
         .vkFormat     = VK_FORMAT_R8_UNORM,
         .vkSrgbFormat = VK_FORMAT_R8_SRGB,

     }},
    {DRM_FORMAT_GR88,
     {
         .drmFormat    = DRM_FORMAT_GR88,
         .vkFormat     = VK_FORMAT_R8G8_UNORM,
         .vkSrgbFormat = VK_FORMAT_R8G8_SRGB,

     }},
    {DRM_FORMAT_RGB888,
     {
         .drmFormat    = DRM_FORMAT_RGB888,
         .vkFormat     = VK_FORMAT_B8G8R8_UNORM,
         .vkSrgbFormat = VK_FORMAT_B8G8R8_SRGB,

     }},
};

SHMFormat NFormatUtils::drmToShm(DRMFormat drm) {
    switch (drm) {
        case DRM_FORMAT_XRGB8888: return WL_SHM_FORMAT_XRGB8888;
        case DRM_FORMAT_ARGB8888: return WL_SHM_FORMAT_ARGB8888;
        default: return drm;
    }

    return drm;
}

DRMFormat NFormatUtils::shmToDRM(SHMFormat shm) {
    switch (shm) {
        case WL_SHM_FORMAT_XRGB8888: return DRM_FORMAT_XRGB8888;
        case WL_SHM_FORMAT_ARGB8888: return DRM_FORMAT_ARGB8888;
        default: return shm;
    }

    return shm;
}

bool NFormatUtils::isFormatYUV(uint32_t drmFormat) {
    switch (drmFormat) {
        case DRM_FORMAT_YUYV:
        case DRM_FORMAT_YVYU:
        case DRM_FORMAT_UYVY:
        case DRM_FORMAT_VYUY:
        case DRM_FORMAT_AYUV:
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_NV21:
        case DRM_FORMAT_NV16:
        case DRM_FORMAT_NV61:
        case DRM_FORMAT_YUV410:
        case DRM_FORMAT_YUV411:
        case DRM_FORMAT_YUV420:
        case DRM_FORMAT_YUV422:
        case DRM_FORMAT_YUV444: return true;
        default: return false;
    }
}

std::string NFormatUtils::drmFormatName(DRMFormat drm) {
    auto n = drmGetFormatName(drm);

    if (!n)
        return "unknown";

    std::string name = n;
    free(n); // NOLINT(cppcoreguidelines-no-malloc,-warnings-as-errors)
    return name;
}

std::string NFormatUtils::drmModifierName(uint64_t mod) {
    auto n = drmGetFormatModifierName(mod);

    if (!n)
        return "unknown";

    std::string name = n;
    free(n); // NOLINT(cppcoreguidelines-no-malloc,-warnings-as-errors)
    return name;
}

DRMFormat NFormatUtils::alphaFormat(DRMFormat prevFormat) {
    switch (prevFormat) {
        case DRM_FORMAT_XRGB8888: return DRM_FORMAT_ARGB8888;
        case DRM_FORMAT_XBGR8888: return DRM_FORMAT_ABGR8888;
        case DRM_FORMAT_BGRX8888: return DRM_FORMAT_BGRA8888;
        case DRM_FORMAT_RGBX8888: return DRM_FORMAT_RGBA8888;
        case DRM_FORMAT_XRGB2101010: return DRM_FORMAT_ARGB2101010;
        case DRM_FORMAT_XBGR2101010: return DRM_FORMAT_ABGR2101010;
        case DRM_FORMAT_RGBX1010102: return DRM_FORMAT_RGBA1010102;
        case DRM_FORMAT_BGRX1010102: return DRM_FORMAT_BGRA1010102;
        default: return 0;
    }
}
