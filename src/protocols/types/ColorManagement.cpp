#include "ColorManagement.hpp"

namespace NColorManagement {
    const SPCPRimaries& getPrimaries(ePrimaries name) {
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

}