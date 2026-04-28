#pragma once

#include "color-management-v1.hpp"
#include <format>
#include <hyprgraphics/color/Color.hpp>
#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/math/Math.hpp"

#include <filesystem>
#include <string>
#include <vector>
#include <expected>

#define SDR_MIN_LUMINANCE 0.2f
#define SDR_MAX_LUMINANCE 80.0f
#define SDR_REF_LUMINANCE 80.0f
#define HDR_MIN_LUMINANCE 0.005f
#define HDR_MAX_LUMINANCE 10000.0f
#define HDR_REF_LUMINANCE 203.0f
#define HLG_MAX_LUMINANCE 1000.0f

namespace Render {
    class ITexture;
}

namespace NColorManagement {
    enum eNoShader : uint8_t {
        CM_NS_DISABLE  = 0,
        CM_NS_ALWAYS   = 1,
        CM_NS_ONDEMAND = 2,
        CM_NS_IGNORE   = 3,
    };

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
        return sc<wpColorManagerV1Primaries>(primaries);
    }
    inline ePrimaries convertPrimaries(wpColorManagerV1Primaries primaries) {
        return sc<ePrimaries>(primaries);
    }
    inline wpColorManagerV1TransferFunction convertTransferFunction(eTransferFunction tf) {
        switch (tf) {
            case CM_TRANSFER_FUNCTION_SRGB: return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_COMPOUND_POWER_2_4;
            default: return sc<wpColorManagerV1TransferFunction>(tf);
        }
    }
    inline eTransferFunction convertTransferFunction(wpColorManagerV1TransferFunction tf) {
        switch (tf) {
            case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_COMPOUND_POWER_2_4: return CM_TRANSFER_FUNCTION_SRGB;
            default: return sc<eTransferFunction>(tf);
        }
    }
    inline std::string tfToString(eTransferFunction tf) {
        switch (tf) {
            case CM_TRANSFER_FUNCTION_BT1886: return "TF:BT1886";
            case CM_TRANSFER_FUNCTION_GAMMA22: return "TF:GAMMA22";
            case CM_TRANSFER_FUNCTION_GAMMA28: return "TF:GAMMA28";
            case CM_TRANSFER_FUNCTION_ST240: return "TF:ST240";
            case CM_TRANSFER_FUNCTION_EXT_LINEAR: return "TF:EXT_LINEAR";
            case CM_TRANSFER_FUNCTION_LOG_100: return "TF:LOG_100";
            case CM_TRANSFER_FUNCTION_LOG_316: return "TF:LOG_316";
            case CM_TRANSFER_FUNCTION_XVYCC: return "TF:XVYCC";
            case CM_TRANSFER_FUNCTION_SRGB: return "TF:SRGB";
            case CM_TRANSFER_FUNCTION_EXT_SRGB: return "TF:EXT_SRGB";
            case CM_TRANSFER_FUNCTION_ST2084_PQ: return "TF:ST2084_PQ";
            case CM_TRANSFER_FUNCTION_ST428: return "TF:ST428";
            case CM_TRANSFER_FUNCTION_HLG: return "TF:HLG";
            default: return "TF:ERROR";
        }
    }

    using SPCPRimaries = Hyprgraphics::SPCPRimaries;

    namespace NColorPrimaries {

        static const auto BT709 = SPCPRimaries{
            .red   = {.x = 0.64, .y = 0.33},
            .green = {.x = 0.30, .y = 0.60},
            .blue  = {.x = 0.15, .y = 0.06},
            .white = {.x = 0.3127, .y = 0.3290},
        };

        static const auto DEFAULT_PRIMARIES = BT709;

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

        static const auto CIE1931_XYZ = SPCPRimaries{
            .red   = {.x = 1.0, .y = 0.0},
            .green = {.x = 0.0, .y = 1.0},
            .blue  = {.x = 0.0, .y = 0.0},
            .white = {.x = 1.0 / 3.0, .y = 1.0 / 3.0},
        };

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

    struct SVCGTTable16 {
        uint16_t                             channels  = 0;
        uint16_t                             entries   = 0;
        uint16_t                             entrySize = 0;
        std::array<std::vector<uint16_t>, 3> ch;
    };

    const SPCPRimaries&   getPrimaries(ePrimaries name);
    std::optional<Mat3x3> rgbToXYZFromPrimaries(SPCPRimaries pr);
    Mat3x3                adaptBradford(Hyprgraphics::CColor::xy srcW, Hyprgraphics::CColor::xy dstW);

    class CPrimaries {
      public:
        static WP<const CPrimaries>   from(const SPCPRimaries& primaries);
        static WP<const CPrimaries>   from(const ePrimaries name);
        static WP<const CPrimaries>   from(const uint primariesId);

        const SPCPRimaries&           value() const;
        uint                          id() const;

        const Hyprgraphics::CMatrix3& toXYZ() const;                                       // toXYZ() * rgb -> xyz
        const Hyprgraphics::CMatrix3& convertMatrix(const WP<const CPrimaries> dst) const; // convertMatrix(dst) * rgb with "this" primaries -> rgb with dst primaries

      private:
        CPrimaries(const SPCPRimaries& primaries, const uint primariesId);
        uint                   m_id;
        SPCPRimaries           m_primaries;

        Hyprgraphics::CMatrix3 m_primaries2XYZ;
    };

    struct SImageDescription {
        static std::expected<SImageDescription, std::string> fromICC(const std::filesystem::path& file);

        //
        std::vector<uint8_t> rawICC;

        eTransferFunction    transferFunction      = CM_TRANSFER_FUNCTION_GAMMA22;
        float                transferFunctionPower = 1.0f;
        bool                 windowsScRGB          = false;

        bool                 primariesNameSet = false;
        ePrimaries           primariesNamed   = CM_PRIMARIES_SRGB;
        // primaries are stored as FP values with the same scale as standard defines (0.0 - 1.0)
        // wayland protocol expects int32_t values multiplied by 1000000
        // drm expects uint16_t values multiplied by 50000
        SPCPRimaries primaries, masteringPrimaries;

        // luminances in cd/m²
        // protos and drm expect min * 10000
        struct SPCLuminances {
            float    min       = 0.2; // 0.2 cd/m²
            uint32_t max       = 80;  // 80 cd/m²
            uint32_t reference = 80;  // 80 cd/m²
            bool     operator==(const SPCLuminances& l2) const {
                return min == l2.min && max == l2.max && reference == l2.reference;
            }
        } luminances;
        struct SPCMasteringLuminances {
            float    min = 0;
            uint32_t max = 0;
            bool     operator==(const SPCMasteringLuminances& l2) const {
                return min == l2.min && max == l2.max;
            }
        } masteringLuminances;

        // Matrix data from ICC
        struct SICCData {
            bool                        present = false;
            size_t                      lutSize = 33;
            std::vector<float>          lutDataPacked;
            SP<Render::ITexture>        lutTexture;
            std::optional<SVCGTTable16> vcgt;
        } icc;

        uint32_t maxCLL  = 0;
        uint32_t maxFALL = 0;

        bool     operator==(const SImageDescription& d2) const {
            if (icc.present || d2.icc.present)
                return false;

            return windowsScRGB == d2.windowsScRGB && transferFunction == d2.transferFunction && transferFunctionPower == d2.transferFunctionPower &&
                (primariesNameSet == d2.primariesNameSet && (primariesNameSet ? primariesNamed == d2.primariesNamed : primaries == d2.primaries)) &&
                masteringPrimaries == d2.masteringPrimaries && luminances == d2.luminances && masteringLuminances == d2.masteringLuminances && maxCLL == d2.maxCLL &&
                maxFALL == d2.maxFALL;
        }

        const SPCPRimaries& getPrimaries() const {
            if (primariesNameSet || primaries == SPCPRimaries{})
                return NColorManagement::getPrimaries(primariesNamed);
            return primaries;
        }

        float getTFMinLuminance(float sdrMinLuminance = -1.0f) const {
            switch (transferFunction) {
                case CM_TRANSFER_FUNCTION_EXT_LINEAR: return 0;
                case CM_TRANSFER_FUNCTION_ST2084_PQ:
                case CM_TRANSFER_FUNCTION_HLG: return HDR_MIN_LUMINANCE;
                case CM_TRANSFER_FUNCTION_BT1886: return 0.01;
                case CM_TRANSFER_FUNCTION_GAMMA22:
                case CM_TRANSFER_FUNCTION_GAMMA28:
                case CM_TRANSFER_FUNCTION_ST240:
                case CM_TRANSFER_FUNCTION_LOG_100:
                case CM_TRANSFER_FUNCTION_LOG_316:
                case CM_TRANSFER_FUNCTION_XVYCC:
                case CM_TRANSFER_FUNCTION_EXT_SRGB:
                case CM_TRANSFER_FUNCTION_ST428:
                case CM_TRANSFER_FUNCTION_SRGB:
                default: return sdrMinLuminance >= 0 ? sdrMinLuminance : SDR_MIN_LUMINANCE;
            }
        };

        float getTFMaxLuminance(int sdrMaxLuminance = -1) const {
            switch (transferFunction) {
                case CM_TRANSFER_FUNCTION_EXT_LINEAR:
                    return SDR_MAX_LUMINANCE; // assume Windows scRGB. white color range 1.0 - 125.0 maps to SDR_MAX_LUMINANCE (80) - HDR_MAX_LUMINANCE (10000)
                case CM_TRANSFER_FUNCTION_ST2084_PQ: return HDR_MAX_LUMINANCE;
                case CM_TRANSFER_FUNCTION_HLG: return HLG_MAX_LUMINANCE;
                case CM_TRANSFER_FUNCTION_BT1886: return 100;
                case CM_TRANSFER_FUNCTION_GAMMA22:
                case CM_TRANSFER_FUNCTION_GAMMA28:
                case CM_TRANSFER_FUNCTION_ST240:
                case CM_TRANSFER_FUNCTION_LOG_100:
                case CM_TRANSFER_FUNCTION_LOG_316:
                case CM_TRANSFER_FUNCTION_XVYCC:
                case CM_TRANSFER_FUNCTION_EXT_SRGB:
                case CM_TRANSFER_FUNCTION_ST428:
                case CM_TRANSFER_FUNCTION_SRGB:
                default: return sdrMaxLuminance >= 0 ? sdrMaxLuminance : SDR_MAX_LUMINANCE;
            }
        };

        float getTFRefLuminance(int sdrRefLuminance = -1) const {
            switch (transferFunction) {
                case CM_TRANSFER_FUNCTION_EXT_LINEAR:
                case CM_TRANSFER_FUNCTION_ST2084_PQ:
                case CM_TRANSFER_FUNCTION_HLG: return HDR_REF_LUMINANCE;
                case CM_TRANSFER_FUNCTION_BT1886: return 100;
                case CM_TRANSFER_FUNCTION_GAMMA22:
                case CM_TRANSFER_FUNCTION_GAMMA28:
                case CM_TRANSFER_FUNCTION_ST240:
                case CM_TRANSFER_FUNCTION_LOG_100:
                case CM_TRANSFER_FUNCTION_LOG_316:
                case CM_TRANSFER_FUNCTION_XVYCC:
                case CM_TRANSFER_FUNCTION_EXT_SRGB:
                case CM_TRANSFER_FUNCTION_ST428:
                case CM_TRANSFER_FUNCTION_SRGB:
                default: return sdrRefLuminance >= 0 ? sdrRefLuminance : SDR_REF_LUMINANCE;
            }
        };
    };

    class CImageDescription {
      public:
        static WP<const CImageDescription> from(const SImageDescription& imageDescription);
        static WP<const CImageDescription> from(const uint64_t imageDescriptionId);

        WP<const CImageDescription>        with(const SImageDescription::SPCLuminances& luminances) const;

        const SImageDescription&           value() const;
        uint64_t                           id() const;

        WP<const CPrimaries>               getPrimaries() const;
        bool                               needsCM(WP<const CImageDescription> target) const;

      private:
        CImageDescription(const SImageDescription& imageDescription, const uint64_t imageDescriptionId);
        uint64_t          m_id          = 0;
        uint              m_primariesId = 0;
        SImageDescription m_imageDescription;
    };

    using PImageDescription = WP<const CImageDescription>;

    PImageDescription getDefaultImageDescription();

    static const auto DEFAULT_GAMMA22_IMAGE_DESCRIPTION = CImageDescription::from(SImageDescription{
        .transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22,
        .primariesNameSet = true,
        .primariesNamed   = NColorManagement::CM_PRIMARIES_SRGB,
        .primaries        = NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_SRGB),
        .luminances       = {.min = SDR_MIN_LUMINANCE, .max = 80, .reference = 80},
    });

    static const auto DEFAULT_SRGB_IMAGE_DESCRIPTION = CImageDescription::from(SImageDescription{
        .transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_SRGB,
        .primariesNameSet = true,
        .primariesNamed   = NColorManagement::CM_PRIMARIES_SRGB,
        .primaries        = NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_SRGB),
        .luminances       = {.min = SDR_MIN_LUMINANCE, .max = 80, .reference = 80},
    });

    static const auto DEFAULT_HDR_IMAGE_DESCRIPTION = CImageDescription::from(SImageDescription{
        .transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ,
        .primariesNameSet = true,
        .primariesNamed   = NColorManagement::CM_PRIMARIES_BT2020,
        .primaries        = NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_BT2020),
        .luminances       = {.min = HDR_MIN_LUMINANCE, .max = 10000, .reference = 203},
    });

    static const auto SCRGB_IMAGE_DESCRIPTION = CImageDescription::from(SImageDescription{
        .transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_EXT_LINEAR,
        .windowsScRGB     = true,
        .primariesNameSet = true,
        .primariesNamed   = NColorManagement::CM_PRIMARIES_SRGB,
        .primaries        = NColorPrimaries::BT709,
        .luminances       = {.reference = 203},
    });

    static const auto LINEAR_IMAGE_DESCRIPTION = CImageDescription::from(SImageDescription{
        .transferFunction = NColorManagement::CM_TRANSFER_FUNCTION_EXT_LINEAR,
        .primariesNameSet = true,
        .primariesNamed   = NColorManagement::CM_PRIMARIES_SRGB,
        .primaries        = NColorPrimaries::BT709,
        .luminances       = {.min = 0, .max = 10000, .reference = 80},
    });
}

template <typename CharT>
struct std::formatter<Hyprgraphics::SPCPRimaries, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const Hyprgraphics::SPCPRimaries& primaries, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "[r={},{} g={},{} b={},{} w={},{}]", primaries.red.x, primaries.red.y, primaries.green.x, primaries.green.y, primaries.blue.x,
                              primaries.blue.y, primaries.white.x, primaries.white.y);
    }
};

template <typename CharT>
struct std::formatter<NColorManagement::SImageDescription::SPCLuminances, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const NColorManagement::SImageDescription::SPCLuminances& luminances, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "[{}-{}({})]", luminances.min, luminances.max, luminances.reference);
    }
};

template <typename CharT>
struct std::formatter<NColorManagement::SImageDescription, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const NColorManagement::SImageDescription& imageDescription, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "[{}{}, primaries={}, luminances={}]", NColorManagement::tfToString(imageDescription.transferFunction),
                              imageDescription.transferFunctionPower != 1.0f ? std::format("^{}", imageDescription.transferFunctionPower) : "", imageDescription.getPrimaries(),
                              imageDescription.luminances);
    }
};
