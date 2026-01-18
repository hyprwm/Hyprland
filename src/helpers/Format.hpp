#pragma once

#include <cstdint>
#include <string>
#include <GLES3/gl32.h>
#include "math/Math.hpp"
#include <aquamarine/backend/Misc.hpp>

using DRMFormat = uint32_t;
using SHMFormat = uint32_t;

#define SWIZZLE_A1GB {GL_ALPHA, GL_ONE, GL_GREEN, GL_BLUE}
#define SWIZZLE_ABG1 {GL_ALPHA, GL_BLUE, GL_GREEN, GL_ONE}
#define SWIZZLE_ABGR {GL_ALPHA, GL_BLUE, GL_GREEN, GL_RED}
#define SWIZZLE_ARGB {GL_ALPHA, GL_RED, GL_GREEN, GL_BLUE}
#define SWIZZLE_B1RG {GL_BLUE, GL_ONE, GL_RED, GL_GREEN}
#define SWIZZLE_BARG {GL_BLUE, GL_ALPHA, GL_RED, GL_GREEN}
#define SWIZZLE_BGR1 {GL_BLUE, GL_GREEN, GL_RED, GL_ONE}
#define SWIZZLE_BGRA {GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA}
#define SWIZZLE_G1AB {GL_GREEN, GL_ONE, GL_ALPHA, GL_BLUE}
#define SWIZZLE_GBA1 {GL_GREEN, GL_BLUE, GL_ALPHA, GL_ONE}
#define SWIZZLE_GBAR {GL_GREEN, GL_BLUE, GL_ALPHA, GL_RED}
#define SWIZZLE_GRAB {GL_GREEN, GL_RED, GL_ALPHA, GL_BLUE}
#define SWIZZLE_R001 {GL_RED, GL_ZERO, GL_ZERO, GL_ONE}
#define SWIZZLE_R1BG {GL_RED, GL_ONE, GL_BLUE, GL_GREEN}
#define SWIZZLE_RABG {GL_RED, GL_ALPHA, GL_BLUE, GL_GREEN}
#define SWIZZLE_RG01 {GL_RED, GL_GREEN, GL_ZERO, GL_ONE}
#define SWIZZLE_GR01 {GL_GREEN, GL_RED, GL_ZERO, GL_ONE}
#define SWIZZLE_RGB1 {GL_RED, GL_GREEN, GL_BLUE, GL_ONE}
#define SWIZZLE_RGBA {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA}

struct SPixelFormat {
    DRMFormat                           drmFormat        = 0; /* DRM_FORMAT_INVALID */
    int                                 glInternalFormat = 0;
    int                                 glFormat         = 0;
    int                                 glType           = 0;
    bool                                withAlpha        = true;
    DRMFormat                           alphaStripped    = 0; /* DRM_FORMAT_INVALID */
    uint32_t                            bytesPerBlock    = 0;
    Vector2D                            blockSize;
    std::optional<std::array<GLint, 4>> swizzle = std::nullopt;
};

using SDRMFormat = Aquamarine::SDRMFormat;

namespace NFormatUtils {
    SHMFormat           drmToShm(DRMFormat drm);
    DRMFormat           shmToDRM(SHMFormat shm);

    const SPixelFormat* getPixelFormatFromDRM(DRMFormat drm);
    const SPixelFormat* getPixelFormatFromGL(uint32_t glFormat, uint32_t glType, bool alpha);
    bool                isFormatYUV(uint32_t drmFormat);
    bool                isFormatOpaque(DRMFormat drm);
    int                 pixelsPerBlock(const SPixelFormat* const fmt);
    int                 minStride(const SPixelFormat* const fmt, int32_t width);
    uint32_t            drmFormatToGL(DRMFormat drm);
    uint32_t            glFormatToType(uint32_t gl);
    std::string         drmFormatName(DRMFormat drm);
    std::string         drmModifierName(uint64_t mod);
    DRMFormat           alphaFormat(DRMFormat prevFormat);
};
