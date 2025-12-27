#include "ColorManagement.hpp"
#include "../../macros.hpp"
#include <hyprutils/memory/UniquePtr.hpp>
#include <map>
#include <vector>

namespace NColorManagement {
    // expected to be small
    static std::vector<UP<const CPrimaries>>                       knownPrimaries;
    static std::vector<UP<const CImageDescription>>                knownDescriptions;
    static std::map<std::pair<uint, uint>, Hyprgraphics::CMatrix3> primariesConversion;

    const SPCPRimaries&                                            getPrimaries(ePrimaries name) {
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

    CPrimaries::CPrimaries(const SPCPRimaries& primaries, const uint primariesId) : m_id(primariesId), m_primaries(primaries) {
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

    WP<const CPrimaries> CPrimaries::from(const uint primariesId) {
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

    CImageDescription::CImageDescription(const SImageDescription& imageDescription, const uint imageDescriptionId) :
        m_id(imageDescriptionId), m_imageDescription(imageDescription) {
        m_primariesId = CPrimaries::from(m_imageDescription.getPrimaries())->id();
    }

    PImageDescription CImageDescription::from(const SImageDescription& imageDescription) {
        for (const auto& known : knownDescriptions) {
            if (known->value() == imageDescription)
                return known;
        }

        knownDescriptions.emplace_back(CUniquePointer(new CImageDescription(imageDescription, knownDescriptions.size() + 1)));
        return knownDescriptions.back();
    }

    PImageDescription CImageDescription::from(const uint imageDescriptionId) {
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

}