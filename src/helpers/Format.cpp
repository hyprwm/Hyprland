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

uint32_t NFormatUtils::drmFormatToGL(DRMFormat drm) {
    switch (drm) {
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_XBGR8888: return GL_RGBA; // doesn't matter, opengl is gucci in this case.
        case DRM_FORMAT_XRGB2101010:
        case DRM_FORMAT_XBGR2101010: return GL_RGB10_A2;
        default: return GL_RGBA;
    }
    UNREACHABLE();
    return GL_RGBA;
}

uint32_t NFormatUtils::glFormatToType(uint32_t gl) {
    return gl != GL_RGBA ? GL_UNSIGNED_INT_2_10_10_10_REV : GL_UNSIGNED_BYTE;
}

std::string NFormatUtils::drmFormatName(DRMFormat drm) {
    auto        n    = drmGetFormatName(drm);
    std::string name = n;
    free(n); // NOLINT(cppcoreguidelines-no-malloc,-warnings-as-errors)
    return name;
}

std::string NFormatUtils::drmModifierName(uint64_t mod) {
    auto        n    = drmGetFormatModifierName(mod);
    std::string name = n;
    free(n); // NOLINT(cppcoreguidelines-no-malloc,-warnings-as-errors)
    return name;
}
