#pragma once

#include "../desktop/DesktopTypes.hpp"

#include <optional>
#include <string>

namespace Animation::Workspace {
    enum eAnimationType : uint8_t {
        ANIMATION_TYPE_IN = 0,
        ANIMATION_TYPE_OUT,
    };

    // TODO: do something about this it sucks
    void startAnimation(PHLWORKSPACE ws, eAnimationType type, bool left = true, bool instant = false, std::optional<std::string> style = std::nullopt);

    void setFullscreenFadeAnimation(PHLWORKSPACE ws, eAnimationType type);
    void setFullscreenFloatingFade(PHLWINDOW pWindow, float fade);
    void overrideFullscreenFadeAmount(PHLWORKSPACE ws, float fade, PHLWINDOW exclude = nullptr);
}
