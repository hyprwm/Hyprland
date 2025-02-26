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

    // NOTE should be ok this way. unsupported primaries/tfs must be rejected earlier. supported enum values should be in sync with proto.
    // might need a proper switch-case and additional INVALID enum value.
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
        static const auto DEFAULT_PRIMARIES = SPCPRimaries{};

        static const auto BT709 = SPCPRimaries{
            .red   = {.x = 0.64, .y = 0.33},
            .green = {.x = 0.30, .y = 0.60},
            .blue  = {.x = 0.15, .y = 0.06},
            .white = {.x = 0.3127, .y = 0.3290},
        };
        static const auto PAL_M = SPCPRimaries{
            .red   = {.x = 0.67, .y = 0.33},
            .green = {.x = 0.21, .y = 0.71},
            .blue  = {.x = 0.14, .y = 0.08},
            .white = {.x = 0.310, .y = 0.316},
        };
        static const auto PAL = SPCPRimaries{
            .red   = {.x = 0.640, .y = 0.330},
            .green = {.x = 0.290, .y = 0.600},
            .blue  = {.x = 0.150, .y = 0.060},
            .white = {.x = 0.3127, .y = 0.3290},
        };
        static const auto NTSC = SPCPRimaries{
            .red   = {.x = 0.630, .y = 0.340},
            .green = {.x = 0.310, .y = 0.595},
            .blue  = {.x = 0.155, .y = 0.070},
            .white = {.x = 0.3127, .y = 0.3290},
        };
        static const auto GENERIC_FILM = SPCPRimaries{
            .red   = {.x = 0.243, .y = 0.692},
            .green = {.x = 0.145, .y = 0.049},
            .blue  = {.x = 0.681, .y = 0.319}, // NOLINT(modernize-use-std-numbers)
            .white = {.x = 0.310, .y = 0.316},
        };
        static const auto BT2020 = SPCPRimaries{
            .red   = {.x = 0.708, .y = 0.292},
            .green = {.x = 0.170, .y = 0.797},
            .blue  = {.x = 0.131, .y = 0.046},
            .white = {.x = 0.3127, .y = 0.3290},
        };

        // FIXME CIE1931_XYZ

        static const auto DCI_P3 = SPCPRimaries{
            .red   = {.x = 0.680, .y = 0.320},
            .green = {.x = 0.265, .y = 0.690},
            .blue  = {.x = 0.150, .y = 0.060},
            .white = {.x = 0.314, .y = 0.351},
        };
        static const auto DISPLAY_P3 = SPCPRimaries{
            .red   = {.x = 0.680, .y = 0.320},
            .green = {.x = 0.265, .y = 0.690},
            .blue  = {.x = 0.150, .y = 0.060},
            .white = {.x = 0.3127, .y = 0.3290},
        };
        static const auto ADOBE_RGB = SPCPRimaries{
            .red   = {.x = 0.6400, .y = 0.3300},
            .green = {.x = 0.2100, .y = 0.7100},
            .blue  = {.x = 0.1500, .y = 0.0600},
            .white = {.x = 0.3127, .y = 0.3290},
        };
    }

    const SPCPRimaries& getPrimaries(ePrimaries name);

    struct SImageDescription {
        uint32_t id = 0; // FIXME needs id setting

        struct SIccFile {
            int      fd     = -1;
            uint32_t length = 0;
            uint32_t offset = 0;
        } icc;

        bool              windowsScRGB = false;

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