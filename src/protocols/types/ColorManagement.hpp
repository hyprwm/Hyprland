#pragma once

#include "xx-color-management-v4.hpp"

struct SImageDescription {
    int                              iccFd   = -1;
    uint32_t                         iccSize = 0;

    xxColorManagerV4TransferFunction transferFunction      = XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB;
    float                            transferFunctionPower = 1.0f;

    bool                             primariesNameSet = false;
    xxColorManagerV4Primaries        primariesNamed   = XX_COLOR_MANAGER_V4_PRIMARIES_SRGB;
    // primaries are stored as FP values with the same scale as standard defines (0.0 - 1.0)
    // wayland protocol expects int32_t values multiplied by 10000
    // drm expects uint16_t values multiplied by 50000
    // frog protocol expects drm values
    struct SPCPRimaries {
        struct {
            float x = 0;
            float y = 0;
        } red, green, blue, white;
    } primaries, masteringPrimaries;

    // luminances in cd/m²
    // protos and drm expect min * 10000
    struct SPCLuminances {
        float    min       = 0.2; // 0.2 cd/m²
        uint32_t max       = 80;  // 80 cd/m²
        uint32_t reference = 80;  // 80 cd/m²
    } luminances;
    struct SPCMasteringLuminances {
        float    min = 0;
        uint32_t max = 0;
    } masteringLuminances;

    uint32_t maxCLL  = 0;
    uint32_t maxFALL = 0;
};

namespace NColorPrimaries {
    static const auto BT709 =
        SImageDescription::SPCPRimaries{.red = {.x = 0.64, .y = 0.33}, .green = {.x = 0.30, .y = 0.60}, .blue = {.x = 0.15, .y = 0.06}, .white = {.x = 0.3127, .y = 0.3290}};

    static const auto BT2020 =
        SImageDescription::SPCPRimaries{.red = {.x = 0.708, .y = 0.292}, .green = {.x = 0.170, .y = 0.797}, .blue = {.x = 0.131, .y = 0.046}, .white = {.x = 0.3127, .y = 0.3290}};
}