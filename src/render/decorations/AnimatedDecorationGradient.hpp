#pragma once

#include <string>

#include "../../config/shared/complex/ComplexDataTypes.hpp"
#include "../../helpers/AnimatedVariable.hpp"

class IHyprWindowDecoration;

class CAnimatedDecorationGradient {
  public:
    struct SRenderState {
        Config::CGradientValueData current;
        Config::CGradientValueData previous;
        float                      progress      = 0.F;
        bool                       transitioning = false;
    };

    CAnimatedDecorationGradient() = default;
    explicit CAnimatedDecorationGradient(Config::CGradientValueData initialGradient);

    void         initializeAnimations(PHLWINDOW window, SP<IHyprWindowDecoration> decoration, const std::string& fadeConfig, const std::string& angleConfig);
    void         setTarget(const Config::CGradientValueData& gradient, bool transition = true);
    SRenderState renderState() const;
    void         onWindowMap();
    void         onWindowFocus();

  private:
    void                       onAngleAnimationEnd(WP<Hyprutils::Animation::CBaseAnimatedVariable> animation);

    Config::CGradientValueData m_current;
    Config::CGradientValueData m_previous;
    PHLANIMVAR<float>          m_fadeProgress;
    PHLANIMVAR<float>          m_angleProgress;
};
