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
}