#include "Format.hpp"
#include <vector>
#include "../includes.hpp"
#include "debug/Log.hpp"
#include "../macros.hpp"
#include <xf86drm.h>
#include <drm_fourcc.h>

/*
    DRM formats are LE, while OGL is BE. The two primary formats
    will be flipped, so we will set flipRB which will later use swizzle
    to flip the red and blue channels.
    This will not work on GLES2, but I want to drop support for it one day anyways.
*/
inline const std::vector<SPixelFormat> GLES3_FORMATS = {
    {
        .drmFormat     = DRM_FORMAT_ARGB8888,
        .flipRB        = true,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_BYTE,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XRGB8888,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat     = DRM_FORMAT_XRGB8888,
        .flipRB        = true,
        .glFormat      = GL_RGBA,
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
        .drmFormat     = DRM_FORMAT_XBGR2101010,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_INT_2_10_10_10_REV,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XBGR2101010,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat     = DRM_FORMAT_ABGR2101010,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_INT_2_10_10_10_REV,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XBGR2101010,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat     = DRM_FORMAT_XRGB2101010,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_INT_2_10_10_10_REV,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XRGB2101010,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat     = DRM_FORMAT_ARGB2101010,
        .glFormat      = GL_RGBA,
        .glType        = GL_UNSIGNED_INT_2_10_10_10_REV,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XRGB2101010,
        .bytesPerBlock = 4,
    },
    {
        .drmFormat     = DRM_FORMAT_XBGR16161616F,
        .glFormat      = GL_RGBA,
        .glType        = GL_HALF_FLOAT,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XBGR16161616F,
        .bytesPerBlock = 8,
    },
    {
        .drmFormat     = DRM_FORMAT_ABGR16161616F,
        .glFormat      = GL_RGBA,
        .glType        = GL_HALF_FLOAT,
        .withAlpha     = true,
        .alphaStripped = DRM_FORMAT_XBGR16161616F,
        .bytesPerBlock = 8,
    },
    {
        .drmFormat     = DRM_FORMAT_XBGR16161616,
        .glFormat      = GL_RGBA16UI,
        .glType        = GL_UNSIGNED_SHORT,
        .withAlpha     = false,
        .alphaStripped = DRM_FORMAT_XBGR16161616,
        .bytesPerBlock = 8,
    },
    {
        .drmFormat     = DRM_FORMAT_ABGR16161616,
        .glFormat      = GL_RGBA16UI,
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

const SPixelFormat* NFormatUtils::getPixelFormatFromDRM(DRMFormat drm) {
    for (auto const& fmt : GLES3_FORMATS) {
        if (fmt.drmFormat == drm)
            return &fmt;
    }

    return nullptr;
}

const SPixelFormat* NFormatUtils::getPixelFormatFromGL(uint32_t glFormat, uint32_t glType, bool alpha) {
    for (auto const& fmt : GLES3_FORMATS) {
        if (fmt.glFormat == static_cast<int>(glFormat) && fmt.glType == static_cast<int>(glType) && fmt.withAlpha == alpha)
            return &fmt;
    }

    return nullptr;
}

bool NFormatUtils::isFormatOpaque(DRMFormat drm) {
    const auto FMT = NFormatUtils::getPixelFormatFromDRM(drm);
    if (!FMT)
        return false;

    return !FMT->withAlpha;
}

int NFormatUtils::pixelsPerBlock(const SPixelFormat* const fmt) {
    return fmt->blockSize.x * fmt->blockSize.y > 0 ? fmt->blockSize.x * fmt->blockSize.y : 1;
}

int NFormatUtils::minStride(const SPixelFormat* const fmt, int32_t width) {
    return std::ceil((width * fmt->bytesPerBlock) / pixelsPerBlock(fmt));
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
    free(n);
    return name;
}

std::string NFormatUtils::drmModifierName(uint64_t mod) {
    auto        n    = drmGetFormatModifierName(mod);
    std::string name = n;
    free(n);
    return name;
}
