#include "ColorManagement.hpp"
#include "../../macros.hpp"
#include <hyprutils/memory/UniquePtr.hpp>
#include <map>
#include <vector>

using namespace NColorManagement;

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

CImageDescription::CImageDescription(const SImageDescription& imageDescription, const uint32_t imageDescriptionId) :
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

PImageDescription CImageDescription::from(const uint32_t imageDescriptionId) {
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

uint CImageDescription::id() const {
    return m_id;
}

WP<const CPrimaries> CImageDescription::getPrimaries() const {
    return CPrimaries::from(m_primariesId);
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