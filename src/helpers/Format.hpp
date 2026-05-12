#pragma once

#include <cstdint>
#include <string>
#include <GLES3/gl32.h>
#include "math/Math.hpp"
#include <aquamarine/backend/Misc.hpp>

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
