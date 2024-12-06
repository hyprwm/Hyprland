#pragma once

#include <cstdint>
#include <string>
#include "math/Math.hpp"
#include <aquamarine/backend/Misc.hpp>

typedef uint32_t DRMFormat;
typedef uint32_t SHMFormat;

struct SPixelFormat {
    DRMFormat drmFormat        = 0; /* DRM_FORMAT_INVALID */
    bool      flipRB           = false;
    int       glInternalFormat = 0;
    int       glFormat         = 0;
    int       glType           = 0;
    bool      withAlpha        = true;
    DRMFormat alphaStripped    = 0; /* DRM_FORMAT_INVALID */
    uint32_t  bytesPerBlock    = 0;
    Vector2D  blockSize;
};

typedef Aquamarine::SDRMFormat SDRMFormat;

namespace NFormatUtils {
    SHMFormat           drmToShm(DRMFormat drm);
    DRMFormat           shmToDRM(SHMFormat shm);

    const SPixelFormat* getPixelFormatFromDRM(DRMFormat drm);
    const SPixelFormat* getPixelFormatFromGL(uint32_t glFormat, uint32_t glType, bool alpha);
    bool                isFormatOpaque(DRMFormat drm);
    int                 pixelsPerBlock(const SPixelFormat* const fmt);
    int                 minStride(const SPixelFormat* const fmt, int32_t width);
    uint32_t            drmFormatToGL(DRMFormat drm);
    uint32_t            glFormatToType(uint32_t gl);
    std::string         drmFormatName(DRMFormat drm);
    std::string         drmModifierName(uint64_t mod);
};
