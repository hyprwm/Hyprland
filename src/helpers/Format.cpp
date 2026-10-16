#include "Format.hpp"
#include <vector>
#include "../includes.hpp"
#include "debug/log/Logger.hpp"
#include "../macros.hpp"
#include <xf86drm.h>
#include <drm_fourcc.h>
#include <hyprgraphics/egl/Egl.hpp>
#include <limits>

using namespace Hyprgraphics::Egl;

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
    switch (drmFormat & ~DRM_FORMAT_BIG_ENDIAN) {
        case DRM_FORMAT_YUYV:
        case DRM_FORMAT_YVYU:
        case DRM_FORMAT_UYVY:
        case DRM_FORMAT_VYUY:
        case DRM_FORMAT_AYUV:
        case DRM_FORMAT_AVUY8888:
        case DRM_FORMAT_XYUV8888:
        case DRM_FORMAT_XVUY8888:
        case DRM_FORMAT_VUY888:
        case DRM_FORMAT_VUY101010:
#ifdef DRM_FORMAT_XVUY2101010
        case DRM_FORMAT_XVUY2101010:
#endif
        case DRM_FORMAT_Y210:
        case DRM_FORMAT_Y212:
        case DRM_FORMAT_Y216:
        case DRM_FORMAT_Y410:
        case DRM_FORMAT_Y412:
        case DRM_FORMAT_Y416:
        case DRM_FORMAT_XVYU2101010:
        case DRM_FORMAT_XVYU12_16161616:
        case DRM_FORMAT_XVYU16161616:
        case DRM_FORMAT_Y0L0:
        case DRM_FORMAT_X0L0:
        case DRM_FORMAT_Y0L2:
        case DRM_FORMAT_X0L2:
        case DRM_FORMAT_YUV420_8BIT:
        case DRM_FORMAT_YUV420_10BIT:
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_NV21:
        case DRM_FORMAT_NV16:
        case DRM_FORMAT_NV61:
        case DRM_FORMAT_NV24:
        case DRM_FORMAT_NV42:
        case DRM_FORMAT_NV15:
        case DRM_FORMAT_NV20:
        case DRM_FORMAT_NV30:
        case DRM_FORMAT_P210:
        case DRM_FORMAT_P010:
        case DRM_FORMAT_P012:
        case DRM_FORMAT_P016:
        case DRM_FORMAT_P030:
#ifdef DRM_FORMAT_P230
        case DRM_FORMAT_P230:
#endif
        case DRM_FORMAT_Q410:
        case DRM_FORMAT_Q401:
#ifdef DRM_FORMAT_T430
        case DRM_FORMAT_T430:
#endif
#ifdef DRM_FORMAT_S010
        case DRM_FORMAT_S010:
        case DRM_FORMAT_S210:
        case DRM_FORMAT_S410:
#endif
#ifdef DRM_FORMAT_S012
        case DRM_FORMAT_S012:
        case DRM_FORMAT_S212:
        case DRM_FORMAT_S412:
#endif
#ifdef DRM_FORMAT_S016
        case DRM_FORMAT_S016:
        case DRM_FORMAT_S216:
        case DRM_FORMAT_S416:
#endif
        case DRM_FORMAT_YUV410:
        case DRM_FORMAT_YVU410:
        case DRM_FORMAT_YUV411:
        case DRM_FORMAT_YVU411:
        case DRM_FORMAT_YUV420:
        case DRM_FORMAT_YVU420:
        case DRM_FORMAT_YUV422:
        case DRM_FORMAT_YVU422:
        case DRM_FORMAT_YUV444:
        case DRM_FORMAT_YVU444: return true;
        default: return false;
    }
}

bool NFormatUtils::isShmBufferLayoutValid(DRMFormat drmFormat, const Vector2D& size, int32_t stride, int32_t offset, size_t poolSize) {
    if (offset < 0 || size.x <= 0 || size.y <= 0 || stride <= 0)
        return false;

    if (size.x > std::numeric_limits<uint32_t>::max() || size.y > std::numeric_limits<uint32_t>::max())
        return false;

    const auto PFORMAT = getPixelFormatFromDRM(drmFormat);
    if (!PFORMAT)
        return false;

    const auto width        = sc<uint32_t>(size.x);
    const auto height       = sc<size_t>(sc<uint32_t>(size.y));
    const auto minStrideVal = sc<size_t>(Hyprgraphics::Egl::minStride(PFORMAT, width));

    if (sc<size_t>(stride) < minStrideVal)
        return false;

    const auto strideBytes = sc<size_t>(stride);
    if (height > 0 && strideBytes > std::numeric_limits<size_t>::max() / height)
        return false;

    const auto dataSize = strideBytes * height;
    const auto offsetSz = sc<size_t>(offset);
    if (offsetSz > std::numeric_limits<size_t>::max() - dataSize)
        return false;

    return offsetSz + dataSize <= poolSize;
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
