#include "ColorManagement.hpp"
#include <map>

namespace NColorManagement {
    static uint32_t                              lastImageID = 0;
    static std::map<uint32_t, SImageDescription> knownDescriptionIds; // expected to be small

    const SPCPRimaries&                          getPrimaries(ePrimaries name) {
        switch (name) {
            case CM_PRIMARIES_SRGB: return NColorPrimaries::BT709;
            case CM_PRIMARIES_BT2020: return NColorPrimaries::BT2020;
            case CM_PRIMARIES_PAL_M: return NColorPrimaries::PAL_M;
            case CM_PRIMARIES_PAL: return NColorPrimaries::PAL;
            case CM_PRIMARIES_NTSC: return NColorPrimaries::NTSC;
            case CM_PRIMARIES_GENERIC_FILM: return NColorPrimaries::GENERIC_FILM;
            case CM_PRIMARIES_CIE1931_XYZ: return NColorPrimaries::DEFAULT_PRIMARIES; // FIXME
            case CM_PRIMARIES_DCI_P3: return NColorPrimaries::DCI_P3;
            case CM_PRIMARIES_DISPLAY_P3: return NColorPrimaries::DISPLAY_P3;
            case CM_PRIMARIES_ADOBE_RGB: return NColorPrimaries::ADOBE_RGB;
            default: return NColorPrimaries::DEFAULT_PRIMARIES;
        }
    }

    // TODO make image descriptions immutable and always set an id

    uint32_t SImageDescription::findId() const {
        for (auto it = knownDescriptionIds.begin(); it != knownDescriptionIds.end(); ++it) {
            if (it->second == *this)
                return it->first;
        }

        const auto newId = ++lastImageID;
        knownDescriptionIds.insert(std::make_pair(newId, *this));
        return newId;
    }

    uint32_t SImageDescription::getId() const {
        return id > 0 ? id : findId();
    }

    uint32_t SImageDescription::updateId() {
        id = 0;
        id = findId();
        return id;
    }

    static uint                                                    lastPrimariesID = 0;
    static std::map<uint, CPrimaries>                              knownPrimaries;
    static std::map<uint, Hyprgraphics::CMatrix3>                  primaries2XYZ;
    static std::map<std::pair<uint, uint>, Hyprgraphics::CMatrix3> primariesConversion;

    const CPrimaries&                                              CPrimaries::from(const SPCPRimaries& primaries) {
        for (auto it = knownPrimaries.begin(); it != knownPrimaries.end(); ++it) {
            if (it->second.primaries() == primaries)
                return it->second;
        }

        lastPrimariesID++;
        knownPrimaries.insert(std::make_pair(lastPrimariesID, CPrimaries(primaries, lastPrimariesID)));
        return knownPrimaries.at(lastPrimariesID);
    }

    const CPrimaries& CPrimaries::from(const ePrimaries name) {
        return from(getPrimaries(name));
    }

    const CPrimaries& CPrimaries::from(const uint primariesId) {
        return knownPrimaries.at(primariesId);
    }

    const Hyprgraphics::CColor::xy& CPrimaries::red() const {
        return m_primaries.red;
    }

    const Hyprgraphics::CColor::xy& CPrimaries::green() const {
        return m_primaries.green;
    }

    const Hyprgraphics::CColor::xy& CPrimaries::blue() const {
        return m_primaries.blue;
    }

    const Hyprgraphics::CColor::xy& CPrimaries::white() const {
        return m_primaries.white;
    }

    const SPCPRimaries& CPrimaries::primaries() const {
        return m_primaries;
    }

    uint CPrimaries::id() const {
        return m_id;
    }

    Hyprgraphics::CMatrix3& CPrimaries::toXYZ() const {
        if (!primaries2XYZ.contains(m_id))
            primaries2XYZ.insert(std::make_pair(m_id, m_primaries.toXYZ()));

        return primaries2XYZ[m_id];
    }

    Hyprgraphics::CMatrix3& CPrimaries::convertMatrix(const CPrimaries& dst) const {
        const auto cacheKey = std::make_pair(m_id, dst.m_id);
        if (!primariesConversion.contains(cacheKey))
            primariesConversion.insert(std::make_pair(cacheKey, m_primaries.convertMatrix(dst.m_primaries)));

        return primariesConversion[cacheKey];
    }

    CPrimaries::CPrimaries(const SPCPRimaries& primaries, const uint primariesId) : m_primaries(primaries), m_id(primariesId) {}
}