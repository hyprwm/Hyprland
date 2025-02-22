#pragma once

#include "color-management-v1.hpp"

namespace NColorManagement {
    enum ePrimaries : uint8_t {
        CM_PRIMARIES_SRGB         = 1,
        CM_PRIMARIES_PAL_M        = 2,
        CM_PRIMARIES_PAL          = 3,
        CM_PRIMARIES_NTSC         = 4,
        CM_PRIMARIES_GENERIC_FILM = 5,
        CM_PRIMARIES_BT2020       = 6,
        CM_PRIMARIES_CIE1931_XYZ  = 7,
        CM_PRIMARIES_DCI_P3       = 8,
        CM_PRIMARIES_DISPLAY_P3   = 9,
        CM_PRIMARIES_ADOBE_RGB    = 10,
    };

    enum eTransferFunction : uint8_t {
        CM_TRANSFER_FUNCTION_BT1886     = 1,
        CM_TRANSFER_FUNCTION_GAMMA22    = 2,
        CM_TRANSFER_FUNCTION_GAMMA28    = 3,
        CM_TRANSFER_FUNCTION_ST240      = 4,
        CM_TRANSFER_FUNCTION_EXT_LINEAR = 5,
        CM_TRANSFER_FUNCTION_LOG_100    = 6,
        CM_TRANSFER_FUNCTION_LOG_316    = 7,
        CM_TRANSFER_FUNCTION_XVYCC      = 8,
        CM_TRANSFER_FUNCTION_SRGB       = 9,
        CM_TRANSFER_FUNCTION_EXT_SRGB   = 10,
        CM_TRANSFER_FUNCTION_ST2084_PQ  = 11,
        CM_TRANSFER_FUNCTION_ST428      = 12,
        CM_TRANSFER_FUNCTION_HLG        = 13,
    };

    // FIXME should be ok this way. unsupported primaries/tfs must be rejected earlier. might need a proper switch-case with exception as default.
    inline wpColorManagerV1Primaries convertPrimaries(ePrimaries primaries) {
        return (wpColorManagerV1Primaries)primaries;
    }
    inline ePrimaries convertPrimaries(wpColorManagerV1Primaries primaries) {
        return (ePrimaries)primaries;
    }
    inline wpColorManagerV1TransferFunction convertTransferFunction(eTransferFunction tf) {
        return (wpColorManagerV1TransferFunction)tf;
    }
    inline eTransferFunction convertTransferFunction(wpColorManagerV1TransferFunction tf) {
        return (eTransferFunction)tf;
    }

    struct SPCPRimaries {
        struct {
            float x = 0;
            float y = 0;
        } red, green, blue, white;
    };

    namespace NColorPrimaries {
        static const auto BT709 = SPCPRimaries{.red = {.x = 0.64, .y = 0.33}, .green = {.x = 0.30, .y = 0.60}, .blue = {.x = 0.15, .y = 0.06}, .white = {.x = 0.3127, .y = 0.3290}};

        static const auto BT2020 =
            SPCPRimaries{.red = {.x = 0.708, .y = 0.292}, .green = {.x = 0.170, .y = 0.797}, .blue = {.x = 0.131, .y = 0.046}, .white = {.x = 0.3127, .y = 0.3290}};
    }

    struct SImageDescription {
        int               iccFd   = -1;
        uint32_t          iccSize = 0;

        eTransferFunction transferFunction      = CM_TRANSFER_FUNCTION_SRGB;
        float             transferFunctionPower = 1.0f;

        bool              primariesNameSet = false;
        ePrimaries        primariesNamed   = CM_PRIMARIES_SRGB;
        // primaries are stored as FP values with the same scale as standard defines (0.0 - 1.0)
        // wayland protocol expects int32_t values multiplied by 10000
        // drm expects uint16_t values multiplied by 50000
        // frog protocol expects drm values
        SPCPRimaries primaries, masteringPrimaries;

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
}