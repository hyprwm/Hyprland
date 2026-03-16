#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "../../desktop/DesktopTypes.hpp"
#include <string>
#include <optional>

class CDesktopAnimationManager {
  public:
    enum eAnimationType : uint8_t {
        ANIMATION_TYPE_IN = 0,
        ANIMATION_TYPE_OUT,
    };

    void startAnimation(PHLWINDOW w, eAnimationType type, bool force = false);
    void startAnimation(PHLLS ls, eAnimationType type, bool instant = false);
    void startAnimation(PHLWORKSPACE ws, eAnimationType type, bool left = true, bool instant = false, std::optional<std::string> style = std::nullopt);

    void setFullscreenFadeAnimation(PHLWORKSPACE ws, eAnimationType type);
    void setFullscreenFloatingFade(PHLWINDOW pWindow, float fade);
    void overrideFullscreenFadeAmount(PHLWORKSPACE ws, float fade, PHLWINDOW exclude = nullptr);

  private:
    void animationPopin(PHLWINDOW w, bool close = false, float minPerc = 0.f);
    void animationSlide(PHLWINDOW w, std::string force = "", bool close = false);
    void animationGnomed(PHLWINDOW w, bool close = false);
};

inline UP<CDesktopAnimationManager> g_pDesktopAnimationManager = makeUnique<CDesktopAnimationManager>();
