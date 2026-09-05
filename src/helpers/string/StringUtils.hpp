#pragma once

#include <aquamarine/backend/Backend.hpp>

namespace StringUtils {
    inline std::string backendStr(Aquamarine::eBackendType t) {
        switch (t) {
            case Aquamarine::AQ_BACKEND_DRM: return "drm";
            case Aquamarine::AQ_BACKEND_HEADLESS: return "headless";
            case Aquamarine::AQ_BACKEND_WAYLAND: return "wayland";
            default: break;
        }
        return "?";
    }
}
