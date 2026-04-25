#include "Format.hpp"
#include <vector>
#include "../includes.hpp"
#include "debug/log/Logger.hpp"
#include "../macros.hpp"
#include <xf86drm.h>
#include <drm_fourcc.h>

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
