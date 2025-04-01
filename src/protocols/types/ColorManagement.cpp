#include "ColorManagement.hpp"
#include <map>

namespace NColorManagement {
    static uint32_t                              lastImageID = 0;
    static std::map<SImageDescription, uint32_t> knownDescriptionIds; // expected to be small1

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

    uint32_t SImageDescription::findId() const {
        const auto known = knownDescriptionIds.find(*this);
        if (known != knownDescriptionIds.end())
            return known->second;
        else {
            const auto newId = ++lastImageID;
            knownDescriptionIds.insert(std::make_pair(*this, newId));
            return newId;
        }
    }

    uint32_t SImageDescription::getId() const {
        return id > 0 ? id : findId();
    }

    uint32_t SImageDescription::updateId() {
        id = findId();
        return id;
    }
}