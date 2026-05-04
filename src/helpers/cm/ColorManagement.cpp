#include "ColorManagement.hpp"
#include "../../macros.hpp"
#include "../Color.hpp"
#include "../TransferFunction.hpp"
#include "../../render/Renderer.hpp"
#include "render/types.hpp"
#include <hyprutils/memory/UniquePtr.hpp>
#include <map>
#include <vector>
#include <numeric>

using namespace NColorManagement;
using namespace NTransferFunction;

namespace NColorManagement {
    // expected to be small
    static std::vector<UP<const CPrimaries>>                       knownPrimaries;
    static std::vector<UP<const CImageDescription>>                knownDescriptions;
    static std::map<std::pair<uint, uint>, Hyprgraphics::CMatrix3> primariesConversion;
}

const SPCPRimaries& NColorManagement::getPrimaries(ePrimaries name) {
    switch (name) {
        case CM_PRIMARIES_SRGB: return NColorPrimaries::BT709;
        case CM_PRIMARIES_BT2020: return NColorPrimaries::BT2020;
        case CM_PRIMARIES_PAL_M: return NColorPrimaries::PAL_M;
        case CM_PRIMARIES_PAL: return NColorPrimaries::PAL;
        case CM_PRIMARIES_NTSC: return NColorPrimaries::NTSC;
        case CM_PRIMARIES_GENERIC_FILM: return NColorPrimaries::GENERIC_FILM;
        case CM_PRIMARIES_CIE1931_XYZ: return NColorPrimaries::CIE1931_XYZ;
        case CM_PRIMARIES_DCI_P3: return NColorPrimaries::DCI_P3;
        case CM_PRIMARIES_DISPLAY_P3: return NColorPrimaries::DISPLAY_P3;
        case CM_PRIMARIES_ADOBE_RGB: return NColorPrimaries::ADOBE_RGB;
        default: return NColorPrimaries::DEFAULT_PRIMARIES;
    }
}

CPrimaries::CPrimaries(const SPCPRimaries& primaries, const uint32_t primariesId) : m_id(primariesId), m_primaries(primaries) {
    m_primaries2XYZ = m_primaries.toXYZ();
}

WP<const CPrimaries> CPrimaries::from(const SPCPRimaries& primaries) {
    for (const auto& known : knownPrimaries) {
        if (known->value() == primaries)
            return known;
    }

    knownPrimaries.emplace_back(CUniquePointer(new CPrimaries(primaries, knownPrimaries.size() + 1)));
    return knownPrimaries.back();
}

WP<const CPrimaries> CPrimaries::from(const ePrimaries name) {
    return from(getPrimaries(name));
}

WP<const CPrimaries> CPrimaries::from(const uint32_t primariesId) {
    ASSERT(primariesId <= knownPrimaries.size());
    return knownPrimaries[primariesId - 1];
}

const SPCPRimaries& CPrimaries::value() const {
    return m_primaries;
}

uint CPrimaries::id() const {
    return m_id;
}

const Hyprgraphics::CMatrix3& CPrimaries::toXYZ() const {
    return m_primaries2XYZ;
}

const Hyprgraphics::CMatrix3& CPrimaries::convertMatrix(const WP<const CPrimaries> dst) const {
    const auto cacheKey = std::make_pair(m_id, dst->m_id);
    if (!primariesConversion.contains(cacheKey))
        primariesConversion.insert(std::make_pair(cacheKey, m_primaries.convertMatrix(dst->m_primaries)));

    return primariesConversion[cacheKey];
}

CImageDescription::CImageDescription(const SImageDescription& imageDescription, const uint64_t imageDescriptionId) :
    m_id(imageDescriptionId), m_imageDescription(imageDescription) {
    m_primariesId = CPrimaries::from(m_imageDescription.getPrimaries())->id();
}

PImageDescription CImageDescription::from(const SImageDescription& imageDescription) {
    for (const auto& known : knownDescriptions) {
        if (known->value() == imageDescription)
            return known;
    }

    knownDescriptions.emplace_back(UP<CImageDescription>(new CImageDescription(imageDescription, knownDescriptions.size() + 1)));
    return knownDescriptions.back();
}

PImageDescription CImageDescription::from(const uint64_t imageDescriptionId) {
    ASSERT(imageDescriptionId <= knownDescriptions.size());
    return knownDescriptions[imageDescriptionId - 1];
}

PImageDescription CImageDescription::with(const SImageDescription::SPCLuminances& luminances) const {
    auto desc       = m_imageDescription;
    desc.luminances = luminances;
    return CImageDescription::from(desc);
}

const SImageDescription& CImageDescription::value() const {
    return m_imageDescription;
}

uint64_t CImageDescription::id() const {
    return m_id;
}

WP<const CPrimaries> CImageDescription::getPrimaries() const {
    return CPrimaries::from(m_primariesId);
}

bool CImageDescription::needsCM(WP<const CImageDescription> target) const {
    if (m_id == target->m_id)
        return false;

    return m_imageDescription.icc.present || target->m_imageDescription.icc.present           // TODO compare ICC
        || m_imageDescription.transferFunction != target->m_imageDescription.transferFunction //
        //|| m_imageDescription.transferFunctionPower != target->m_imageDescription.transferFunctionPower // TODO unsupported
        || m_imageDescription.getPrimaries() != target->m_imageDescription.getPrimaries() //
        // || m_imageDescription.masteringPrimaries != target->m_imageDescription.masteringPrimaries // TODO unused
        || m_imageDescription.luminances != target->m_imageDescription.luminances //
        // || m_imageDescription.masteringLuminances != target->m_imageDescription.masteringLuminances // TODO unused
        ;
}

static Mat3x3 diag3(const std::array<float, 3>& s) {
    return Mat3x3{std::array<float, 9>{s[0], 0, 0, 0, s[1], 0, 0, 0, s[2]}};
}

static std::optional<Mat3x3> invertMat3(const Mat3x3& m) {
    const auto   ARR = m.getMatrix();
    const double a = ARR[0], b = ARR[1], c = ARR[2];
    const double d = ARR[3], e = ARR[4], f = ARR[5];
    const double g = ARR[6], h = ARR[7], i = ARR[8];

    const double A = (e * i - f * h);
    const double B = -(d * i - f * g);
    const double C = (d * h - e * g);
    const double D = -(b * i - c * h);
    const double E = (a * i - c * g);
    const double F = -(a * h - b * g);
    const double G = (b * f - c * e);
    const double H = -(a * f - c * d);
    const double I = (a * e - b * d);

    const double det = a * A + b * B + c * C;
    if (std::abs(det) < 1e-18)
        return std::nullopt;

    const double invDet = 1.0 / det;
    Mat3x3       inv{std::array<float, 9>{
        A * invDet,
        D * invDet,
        G * invDet, //
        B * invDet,
        E * invDet,
        H * invDet, //
        C * invDet,
        F * invDet,
        I * invDet, //
    }};
    return inv;
}

static std::array<float, 3> matByVec(const Mat3x3& M, const std::array<float, 3>& v) {
    const auto ARR = M.getMatrix();
    return {ARR[0] * v[0] + ARR[1] * v[1] + ARR[2] * v[2], ARR[3] * v[0] + ARR[4] * v[1] + ARR[5] * v[2], ARR[6] * v[0] + ARR[7] * v[1] + ARR[8] * v[2]};
}

std::optional<Mat3x3> NColorManagement::rgbToXYZFromPrimaries(SPCPRimaries pr) {
    const auto R = Hyprgraphics::xy2xyz(pr.red);
    const auto G = Hyprgraphics::xy2xyz(pr.green);
    const auto B = Hyprgraphics::xy2xyz(pr.blue);
    const auto W = Hyprgraphics::xy2xyz(pr.white);

    // P has columns R,G,B
    Mat3x3 P{std::array<float, 9>{R.x, G.x, B.x, R.y, G.y, B.y, R.z, G.z, B.z}};

    auto   invP = invertMat3(P);
    if (!invP)
        return std::nullopt;

    const auto S = matByVec(*invP, {W.x, W.y, W.z});

    P.multiply(diag3(S)); // RGB->XYZ

    return P;
}

Mat3x3 NColorManagement::adaptBradford(Hyprgraphics::CColor::xy srcW, Hyprgraphics::CColor::xy dstW) {
    static const Mat3x3        Bradford{std::array<float, 9>{0.8951f, 0.2664f, -0.1614f, -0.7502f, 1.7135f, 0.0367f, 0.0389f, -0.0685f, 1.0296f}};
    static const Mat3x3        BradfordInv = invertMat3(Bradford).value();

    const auto                 srcXYZ = Hyprgraphics::xy2xyz(srcW);
    const auto                 dstXYZ = Hyprgraphics::xy2xyz(dstW);

    const auto                 srcLMS = matByVec(Bradford, {srcXYZ.x, srcXYZ.y, srcXYZ.z});
    const auto                 dstLMS = matByVec(Bradford, {dstXYZ.x, dstXYZ.y, dstXYZ.z});

    const std::array<float, 3> scale{dstLMS[0] / srcLMS[0], dstLMS[1] / srcLMS[1], dstLMS[2] / srcLMS[2]};

    Mat3x3                     result = BradfordInv;
    result.multiply(diag3(scale)).multiply(Bradford);

    return result;
}

// sRGB constants
#define SRGB_POW   2.4
#define SRGB_CUT   0.0031308
#define SRGB_SCALE 12.92
#define SRGB_ALPHA 1.055

#define BT1886_POW   (1.0 / 0.45)
#define BT1886_CUT   0.018053968510807
#define BT1886_SCALE 4.5
#define BT1886_ALPHA (1.0 + 5.5 * BT1886_CUT)

// See http://car.france3.mars.free.fr/HD/INA-%2026%20jan%2006/SMPTE%20normes%20et%20confs/s240m.pdf
#define ST240_POW   (1.0 / 0.45)
#define ST240_CUT   0.0228
#define ST240_SCALE 4.0
#define ST240_ALPHA 1.1115

#define ST428_POW   2.6
#define ST428_SCALE (52.37 / 48.0)

// PQ constants
#define PQ_M1     0.1593017578125
#define PQ_M2     78.84375
#define PQ_INV_M1 (1.0 / PQ_M1)
#define PQ_INV_M2 (1.0 / PQ_M2)
#define PQ_C1     0.8359375
#define PQ_C2     18.8515625
#define PQ_C3     18.6875

// HLG constants
#define HLG_D_CUT (1.0 / 12.0)
#define HLG_E_CUT 0.5
#define HLG_A     0.17883277
#define HLG_B     0.28466892
#define HLG_C     0.55991073

static inline int sign(double value) {
    return value < 0 ? -1 : 1;
}

// The primary source for these transfer functions is https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.1361-0-199802-W!!PDF-E.pdf
static RGBAColor tfInvPQ(RGBAColor color) {
    for (uint i = 0; i <= 2; i++) {
        const double E = pow(std::clamp(color.v[i], 0.0, 1.0), PQ_INV_M2);
        color.v[i]     = pow((std::max(E - PQ_C1, 0.0)) / (PQ_C2 - PQ_C3 * E), PQ_INV_M1);
    }
    return color;
}

static RGBAColor tfInvHLG(RGBAColor color) {
    for (uint i = 0; i <= 2; i++) {
        const bool isLow = color.v[i] <= HLG_E_CUT;
        color.v[i]       = isLow ? color.v[i] * color.v[i] / 3.0 : (exp((color.v[i] - HLG_C) / HLG_A) + HLG_B) / 12.0;
    }
    return color;
}

// Many transfer functions (including sRGB) follow the same pattern: a linear
// segment for small values and a power function for larger values. The
// following function implements this pattern from which sRGB, BT.1886, and
// others can be derived by plugging in the right constants.
static RGBAColor tfInvLinPow(RGBAColor color, float gamma, float thres, float scale, float alpha) {
    for (uint i = 0; i <= 2; i++) {
        const bool isLow = color.v[i] <= thres * scale;
        color.v[i]       = isLow ? color.v[i] / scale : pow((color.v[i] + alpha - 1.0) / alpha, gamma);
    }
    return color;
}

static RGBAColor tfInvSRGB(RGBAColor color) {
    return tfInvLinPow(color, SRGB_POW, SRGB_CUT, SRGB_SCALE, SRGB_ALPHA);
}

static RGBAColor tfInvExtSRGB(RGBAColor color) {
    // EXT sRGB is the sRGB transfer function mirrored around 0.
    const auto absColor = tfInvSRGB({{.r = abs(color.c.r), .g = abs(color.c.g), .b = abs(color.c.b), .a = color.c.a}});
    return {{
        .r = absColor.c.r * sign(color.c.r),
        .g = absColor.c.g * sign(color.c.g),
        .b = absColor.c.b * sign(color.c.b),
        .a = absColor.c.a,
    }};
}

static RGBAColor tfInvBT1886(RGBAColor color) {
    return tfInvLinPow(color, BT1886_POW, BT1886_CUT, BT1886_SCALE, BT1886_ALPHA);
}

static RGBAColor tfInvXVYCC(RGBAColor color) {
    // The inverse transfer function for XVYCC is the BT1886 transfer function mirrored around 0,
    // same as what EXT sRGB is to sRGB.
    const auto absColor = tfInvBT1886({{.r = abs(color.c.r), .g = abs(color.c.g), .b = abs(color.c.b), .a = color.c.a}});
    return {{
        .r = absColor.c.r * sign(color.c.r),
        .g = absColor.c.g * sign(color.c.g),
        .b = absColor.c.b * sign(color.c.b),
        .a = absColor.c.a,
    }};
}

static RGBAColor tfInvST240(RGBAColor color) {
    return tfInvLinPow(color, ST240_POW, ST240_CUT, ST240_SCALE, ST240_ALPHA);
}

// Forward transfer functions corresponding to the inverse functions above.
static RGBAColor tfPQ(RGBAColor color) {
    for (uint i = 0; i <= 2; i++) {
        const double E = pow(std::clamp(color.v[i], 0.0, 1.0), PQ_M1);
        color.v[i]     = pow((PQ_C1 + PQ_C2 * E) / (1.0 + PQ_C3 * E), PQ_M2);
    }
    return color;
}

static RGBAColor tfHLG(RGBAColor color) {
    for (uint i = 0; i <= 2; i++) {
        const bool isLow = color.v[i] <= HLG_D_CUT;
        color.v[i]       = isLow ? sqrt(std::max(color.v[i], 0.0) * 3.0) : HLG_A * log(std::max(12.0 * color.v[i] - HLG_B, 0.0001)) + HLG_C;
    }
    return color;
}

static RGBAColor tfLinPow(RGBAColor color, float gamma, float thres, float scale, float alpha) {
    for (uint i = 0; i <= 2; i++) {
        const bool isLow = color.v[i] <= thres;
        color.v[i]       = isLow ? color.v[i] * scale : pow(color.v[i], 1.0 / gamma) * alpha - (alpha - 1.0);
    }
    return color;
}

static RGBAColor tfSRGB(RGBAColor color) {
    return tfLinPow(color, SRGB_POW, SRGB_CUT, SRGB_SCALE, SRGB_ALPHA);
}

static RGBAColor tfExtSRGB(RGBAColor color) {
    // EXT sRGB is the sRGB transfer function mirrored around 0.
    const auto absColor = tfSRGB({{.r = abs(color.c.r), .g = abs(color.c.g), .b = abs(color.c.b), .a = color.c.a}});
    return {{
        .r = absColor.c.r * sign(color.c.r),
        .g = absColor.c.g * sign(color.c.g),
        .b = absColor.c.b * sign(color.c.b),
        .a = absColor.c.a,
    }};
}

static RGBAColor tfBT1886(RGBAColor color) {
    return tfLinPow(color, BT1886_POW, BT1886_CUT, BT1886_SCALE, BT1886_ALPHA);
}

static RGBAColor tfXVYCC(RGBAColor color) {
    // The transfer function for XVYCC is the BT1886 transfer function mirrored around 0,
    // same as what EXT sRGB is to sRGB.
    const auto absColor = tfBT1886({{.r = abs(color.c.r), .g = abs(color.c.g), .b = abs(color.c.b), .a = color.c.a}});
    return {{
        .r = absColor.c.r * sign(color.c.r),
        .g = absColor.c.g * sign(color.c.g),
        .b = absColor.c.b * sign(color.c.b),
        .a = absColor.c.a,
    }};
}

static RGBAColor tfST240(RGBAColor color) {
    return tfLinPow(color, ST240_POW, ST240_CUT, ST240_SCALE, ST240_ALPHA);
}

static RGBAColor tfST428(RGBAColor color) {
    for (uint i = 0; i <= 2; i++) {
        color.v[i] = pow(std::max(color.v[i], 0.0), ST428_POW) * ST428_SCALE;
    }
    return color;
}

static RGBAColor tfInvST428(RGBAColor color) {
    for (uint i = 0; i <= 2; i++) {
        color.v[i] = pow(std::max(color.v[i], 0.0) / ST428_SCALE, 1.0 / ST428_POW);
    }
    return color;
}

static RGBAColor tfGamma(RGBAColor color, float gamma) {
    for (uint i = 0; i <= 2; i++) {
        color.v[i] = pow(std::max(color.v[i], 0.0), gamma);
    }
    return color;
}

static RGBAColor tfLog(RGBAColor color, float mult) {
    for (uint i = 0; i <= 2; i++) {
        color.v[i] = color.v[i] <= 0 ? 0.0 : exp((color.v[i] - 1.0) * mult * std::numbers::ln10);
    }
    return color;
}

static RGBAColor tfInvLog(RGBAColor color, float mult, float min) {
    for (uint i = 0; i <= 2; i++) {
        color.v[i] = color.v[i] <= min ? 0.0 : 1.0 + log(color.v[i]) / std::numbers::ln10 / mult;
    }
    return color;
}

static RGBAColor toLinearRGB(RGBAColor color, eTransferFunction tf) {
    switch (tf) {
        case CM_TRANSFER_FUNCTION_LINEAR: return color;
        case CM_TRANSFER_FUNCTION_EXT_LINEAR: return color;
        case CM_TRANSFER_FUNCTION_ST2084_PQ: return tfInvPQ(color);
        case CM_TRANSFER_FUNCTION_GAMMA22: return tfGamma(color, 2.2);
        case CM_TRANSFER_FUNCTION_GAMMA28: return tfGamma(color, 2.8);
        case CM_TRANSFER_FUNCTION_HLG: return tfInvHLG(color);
        case CM_TRANSFER_FUNCTION_EXT_SRGB: return tfInvExtSRGB(color);
        case CM_TRANSFER_FUNCTION_BT1886: return tfInvBT1886(color);
        case CM_TRANSFER_FUNCTION_ST240: return tfInvST240(color);
        case CM_TRANSFER_FUNCTION_LOG_100: return tfLog(color, 2.0);
        case CM_TRANSFER_FUNCTION_LOG_316: return tfLog(color, 2.5);
        case CM_TRANSFER_FUNCTION_XVYCC: return tfInvXVYCC(color);
        case CM_TRANSFER_FUNCTION_ST428: return tfST428(color);
        case CM_TRANSFER_FUNCTION_SRGB:
        default: return tfInvSRGB(color);
    }
}

static RGBAColor fromLinearRGB(RGBAColor color, eTransferFunction tf) {
    switch (tf) {
        case CM_TRANSFER_FUNCTION_EXT_LINEAR: return color;
        case CM_TRANSFER_FUNCTION_ST2084_PQ: return tfPQ(color);
        case CM_TRANSFER_FUNCTION_GAMMA22: return tfGamma(color, 1.0 / 2.2);
        case CM_TRANSFER_FUNCTION_GAMMA28: return tfGamma(color, 1.0 / 2.8);
        case CM_TRANSFER_FUNCTION_HLG: return tfHLG(color);
        case CM_TRANSFER_FUNCTION_EXT_SRGB: return tfExtSRGB(color);
        case CM_TRANSFER_FUNCTION_BT1886: return tfBT1886(color);
        case CM_TRANSFER_FUNCTION_ST240: return tfST240(color);
        case CM_TRANSFER_FUNCTION_LOG_100: return tfInvLog(color, 2.0, 0.01);
        case CM_TRANSFER_FUNCTION_LOG_316: return tfInvLog(color, 2.5, sqrt(10.0) / 1000.0);
        case CM_TRANSFER_FUNCTION_XVYCC: return tfXVYCC(color);
        case CM_TRANSFER_FUNCTION_ST428: return tfInvST428(color);
        case CM_TRANSFER_FUNCTION_SRGB:
        default: return tfSRGB(color);
    }
}

static RGBAColor toNit(RGBAColor color, Render::STFRange range) {
    for (uint i = 0; i <= 2; i++) {
        color.v[i] = color.v[i] * (range.max - range.min) + range.min;
    }
    return color;
}

static RGBAColor fromLinearNit(RGBAColor color, eTransferFunction tf, Render::STFRange range) {
    if (tf == CM_TRANSFER_FUNCTION_LINEAR)
        return color;

    for (uint i = 0; i <= 2; i++) {
        color.v[i] = (color.v[i] - range.min * color.c.a) / (range.max - range.min);
    }
    color /= std::max(color.c.a, 0.001);
    color = fromLinearRGB(color, tf);
    color *= color.c.a;
    return color;
}

static RGBAColor saturate(RGBAColor color, std::array<std::array<double, 3>, 3> primaries, float saturation) {
    if (saturation == 1.0)
        return color;

    std::array<double, 3> colorArr   = {color.v[0], color.v[1], color.v[2]};
    std::array<double, 3> brightness = {primaries[1][0], primaries[1][1], primaries[1][2]};
    const auto            Y          = std::inner_product(colorArr.begin(), colorArr.end(), brightness.begin(), 0.0);
    for (uint i = 0; i <= 2; i++) {
        color.v[i] = (color.v[i] * saturation) + (Y * (1.0 - saturation));
    }
    return color;
}

static RGBAColor tonemap(RGBAColor color, std::array<std::array<double, 3>, 3> dstXYZ, float maxLuminance, float dstMaxLuminance, float dstRefLuminance, float srcRefLuminance) {
    // TODO source color is expected to be in sRGB colorspace and tonamepping shouldn't be needed
    return color;
}

RGBAColor NColorManagement::convertColor(RGBAColor color, PImageDescription srcDesc, PImageDescription dstDesc) {
    const auto settings =
        g_pHyprRenderer->getCMSettings(srcDesc, dstDesc, nullptr, true, g_pHyprRenderer->m_renderData.pMonitor ? g_pHyprRenderer->m_renderData.pMonitor->m_sdrMinLuminance : -1,
                                       g_pHyprRenderer->m_renderData.pMonitor ? g_pHyprRenderer->m_renderData.pMonitor->m_sdrMaxLuminance : -1);

    color /= std::max(color.c.a, 0.001);
    color = toLinearRGB(color, srcDesc->value().transferFunction);
    if (dstDesc->value().icc.present) {
        // color.rgb = applyIcc3DLut(color.rgb, iccLut3D, iccLutSize);
        color *= color.c.a;
    } else {
        const auto convertMatrix = srcDesc->getPrimaries()->convertMatrix(dstDesc->getPrimaries());
        const auto converted     = convertMatrix * Hyprgraphics::CColor::XYZ{.x = color.c.r, .y = color.c.g, .z = color.c.b};
        color.c.r                = converted.x;
        color.c.g                = converted.y;
        color.c.b                = converted.z;
        if (srcDesc->value().transferFunction != CM_TRANSFER_FUNCTION_LINEAR)
            color = toNit(color, settings.srcTFRange);
        color *= color.c.a;
        if (settings.needsTonemap)
            color = tonemap(color, settings.dstPrimaries2XYZ, settings.maxLuminance, settings.dstMaxLuminance, settings.dstRefLuminance, settings.srcRefLuminance);

        color = fromLinearNit(color, dstDesc->value().transferFunction, settings.dstTFRange);
        if (settings.needsSDRmod) {
            color = saturate(color, settings.dstPrimaries2XYZ, settings.sdrSaturation);
            color *= settings.sdrBrightnessMultiplier;
        }
    }
    return color;
}

CHyprColor NColorManagement::convertColor(const CHyprColor& color, PImageDescription srcDesc, PImageDescription dstDesc) {
    const auto& converted = convertColor(RGBAColor{{.r = color.r, .g = color.g, .b = color.b, .a = color.a}}, srcDesc, dstDesc);
    return CHyprColor(converted.c.r, converted.c.g, converted.c.b, converted.c.a);
}

PImageDescription NColorManagement::getDefaultImageDescription() {
    const auto TF = fromConfig();
    switch (TF) {
        case TF_AUTO:
        case TF_GAMMA22:
        case TF_FORCED_GAMMA22: return DEFAULT_GAMMA22_IMAGE_DESCRIPTION;
        case TF_DEFAULT:
        case TF_SRGB: return DEFAULT_SRGB_IMAGE_DESCRIPTION;
        default: UNREACHABLE();
    }
}
