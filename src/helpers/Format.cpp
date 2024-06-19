#include "Format.hpp"
#include <vector>
#include "../includes.hpp"
#include "debug/Log.hpp"
#include "../macros.hpp"

/*
    DRM formats are LE, while OGL is BE. The two primary formats
    will be flipped, so we will set flipRB which will later use swizzle
    to flip the red and blue channels.
    This will not work on GLES2, but I want to drop support for it one day anyways.
*/
inline const std::vector<SPixelFormat> GLES3_FORMATS = {
    {
        .drmFormat = DRM_FORMAT_ARGB8888,
        .flipRB    = true,
#ifndef GLES2
        .glFormat = GL_RGBA,
#else
        .glFormat = GL_BGRA_EXT,
#endif
        .glType        = GL_UNSIGNED_BYTE,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XRGB8888,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat = DRM_FORMAT_XRGB8888,
        .flipRB    = true,
#ifndef GLES2
        .glFormat = GL_RGBA,
#else
        .glFormat = GL_BGRA_EXT,
#endif
        .glType        = GL_UNSIGNED_BYTE,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XRGB8888,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat     = DRM_FORMAT_XBGR8888,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_BYTE,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XBGR8888,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat     = DRM_FORMAT_ABGR8888,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_BYTE,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XBGR8888,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat     = DRM_FORMAT_BGR888,
        .glFormat      = GL_RGB,
        .glType        = GL_UNSIGNED_BYTE,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_BGR888,
        .bytesPerBlock = 3,
    },
    {
        .drmFormat     = DRM_FORMAT_RGBX4444,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_SHORT_4_4_4_4,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_RGBX4444,
        .bytesPerBlock = 2,
    },
    {
        .drmFormat     = DRM_FORMAT_RGBA4444,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_SHORT_4_4_4_4,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_RGBX4444,
        .bytesPerBlock = 2,
    },
    {
        .drmFormat     = DRM_FORMAT_RGBX5551,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_SHORT_5_5_5_1,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_RGBX5551,
        .bytesPerBlock = 2,
    },
    {
        .drmFormat     = DRM_FORMAT_RGBA5551,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_SHORT_5_5_5_1,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_RGBX5551,
        .bytesPerBlock = 2,
    },
    {
        .drmFormat     = DRM_FORMAT_RGB565,
        .glFormat      = GL_RGB,
        .glType        = GL_UNSIGNED_SHORT_5_6_5,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_RGB565,
        .bytesPerBlock = 2,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR2101010,
        .glFormat  = GL_RGBA,
#ifndef GLES2
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV,
#else
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
#endif
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XBGR2101010,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR2101010,
        .glFormat  = GL_RGBA,
#ifndef GLES2
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV,
#else
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
#endif
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XBGR2101010,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat = DRM_FORMAT_XRGB2101010,
        .glFormat  = GL_RGBA,
#ifndef GLES2
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV,
#else
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
#endif
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XRGB2101010,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat = DRM_FORMAT_ARGB2101010,
        .glFormat  = GL_RGBA,
#ifndef GLES2
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV,
#else
        .glType = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
#endif
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XRGB2101010,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR16161616F,
        .glFormat  = GL_RGBA,
#ifndef GLES2
        .glType = GL_HALF_FLOAT,
#else
        .glType = GL_HALF_FLOAT_OES,
#endif
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XBGR16161616F,
        .bytesPerBlock = 8,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR16161616F,
        .glFormat  = GL_RGBA,
#ifndef GLES2
        .glType = GL_HALF_FLOAT,
#else
        .glType = GL_HALF_FLOAT_OES,
#endif
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XBGR16161616F,
        .bytesPerBlock = 8,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR16161616,
#ifndef GLES2
        .glFormat = GL_RGBA16UI,
#else
        .glFormat = GL_RGBA16_EXT,
#endif
        .glType        = GL_UNSIGNED_SHORT,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XBGR16161616,
        .bytesPerBlock = 8,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR16161616,
#ifndef GLES2
        .glFormat = GL_RGBA16UI,
#else
        .glFormat = GL_RGBA16_EXT,
#endif
        .glType        = GL_UNSIGNED_SHORT,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XBGR16161616,
        .bytesPerBlock = 8,
    },
    {
        .drmFormat     = DRM_FORMAT_YVYU,
        .bytesPerBlock = 4,
        .blockSize     = {2, 1},
    },
    {
        .drmFormat     = DRM_FORMAT_VYUY,
        .bytesPerBlock = 4,
        .blockSize     = {2, 1},
    },
    {
        .drmFormat     = DRM_FORMAT_R8,
        .bytesPerBlock = 1,
    },
    {
        .drmFormat     = DRM_FORMAT_GR88,
        .bytesPerBlock = 2,
    },
    {
        .drmFormat     = DRM_FORMAT_RGB888,
        .bytesPerBlock = 3,
    },
    {
        .drmFormat     = DRM_FORMAT_BGR888,
        .bytesPerBlock = 3,
    },
    {
        .drmFormat     = DRM_FORMAT_RGBX4444,
        .bytesPerBlock = 2,
    },
};

SHMFormat FormatUtils::drmToShm(DRMFormat drm) {
    switch (drm) {
        case DRM_FORMAT_XRGB8888: return WL_SHM_FORMAT_XRGB8888;
        case DRM_FORMAT_ARGB8888: return WL_SHM_FORMAT_ARGB8888;
        default: return drm;
    }

    return drm;
}

DRMFormat FormatUtils::shmToDRM(SHMFormat shm) {
    switch (shm) {
        case WL_SHM_FORMAT_XRGB8888: return DRM_FORMAT_XRGB8888;
        case WL_SHM_FORMAT_ARGB8888: return DRM_FORMAT_ARGB8888;
        default: return shm;
    }

    return shm;
}

const SPixelFormat* FormatUtils::getPixelFormatFromDRM(DRMFormat drm) {
    for (auto& fmt : GLES3_FORMATS) {
        if (fmt.drmFormat == drm)
            return &fmt;
    }

    return nullptr;
}

const SPixelFormat* FormatUtils::getPixelFormatFromGL(uint32_t glFormat, uint32_t glType, bool alpha) {
    for (auto& fmt : GLES3_FORMATS) {
        if (fmt.glFormat == (int)glFormat && fmt.glType == (int)glType && fmt.withAlpha == alpha)
            return &fmt;
    }

    return nullptr;
}

bool FormatUtils::isFormatOpaque(DRMFormat drm) {
    const auto FMT = FormatUtils::getPixelFormatFromDRM(drm);
    if (!FMT)
        return false;

    return !FMT->withAlpha;
}

int FormatUtils::pixelsPerBlock(const SPixelFormat* const fmt) {
    return fmt->blockSize.x * fmt->blockSize.y > 0 ? fmt->blockSize.x * fmt->blockSize.y : 1;
}

int FormatUtils::minStride(const SPixelFormat* const fmt, int32_t width) {
    return std::ceil((width * fmt->bytesPerBlock) / pixelsPerBlock(fmt));
}

uint32_t FormatUtils::drmFormatToGL(DRMFormat drm) {
    switch (drm) {
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_XBGR8888: return GL_RGBA; // doesn't matter, opengl is gucci in this case.
        case DRM_FORMAT_XRGB2101010:
        case DRM_FORMAT_XBGR2101010:
#ifdef GLES2
            return GL_RGB10_A2_EXT;
#else
            return GL_RGB10_A2;
#endif
        default: return GL_RGBA;
    }
    UNREACHABLE();
    return GL_RGBA;
}

uint32_t FormatUtils::glFormatToType(uint32_t gl) {
    return gl != GL_RGBA ?
#ifdef GLES2
        GL_UNSIGNED_INT_2_10_10_10_REV_EXT :
#else
        GL_UNSIGNED_INT_2_10_10_10_REV :
#endif
        GL_UNSIGNED_BYTE;
}
