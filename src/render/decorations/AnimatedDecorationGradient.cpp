#include "AnimatedDecorationGradient.hpp"

#include <utility>

#include "../../animation/AnimationManager.hpp"
#include "../../config/shared/animation/AnimationTree.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include "IHyprWindowDecoration.hpp"

using namespace Hyprutils::Animation;

CAnimatedDecorationGradient::CAnimatedDecorationGradient(Config::CGradientValueData initialGradient) : m_current(std::move(initialGradient)), m_previous(m_current) {
    ;
}

void CAnimatedDecorationGradient::initializeAnimations(PHLWINDOW window, SP<IHyprWindowDecoration> decoration, const std::string& fadeConfig, const std::string& angleConfig) {
    Animation::mgr()->createAnimation(0.F, m_fadeProgress, Config::animationTree()->getAnimationPropertyConfig(fadeConfig), window, decoration, AVARDAMAGE_DECORATION);
    Animation::mgr()->createAnimation(0.F, m_angleProgress, Config::animationTree()->getAnimationPropertyConfig(angleConfig), window, decoration, AVARDAMAGE_DECORATION);
}

void CAnimatedDecorationGradient::setTarget(const Config::CGradientValueData& gradient, bool transition) {
    if (gradient == m_current)
        return;

    const bool WAS_WHOLE_MONITOR = m_current.m_wholeMonitor;
    if (transition)
        m_previous = gradient.m_wholeMonitor && !WAS_WHOLE_MONITOR ? renderState().current : m_current;

    m_current = gradient;

    if (m_current.m_wholeMonitor && m_angleProgress) {
        m_angleProgress->resetAllCallbacks();
        m_angleProgress->warp(false);
    } else if (WAS_WHOLE_MONITOR)
        startAngleAnimation();

    if (!transition)
        return;

    m_fadeProgress->setValueAndWarp(0.F);
    *m_fadeProgress = 1.F;
}

CAnimatedDecorationGradient::SRenderState CAnimatedDecorationGradient::renderState() const {
    SRenderState state = {
        .current       = m_current,
        .previous      = m_previous,
        .progress      = m_fadeProgress->value(),
        .transitioning = m_fadeProgress->isBeingAnimated(),
    };

    if (m_current.m_wholeMonitor || !m_angleProgress->enabled())
        return state;

    state.current.m_angle += m_angleProgress->value() * M_PI * 2;
    state.current.m_angle = normalizeAngleRad(state.current.m_angle);

    // Rotating gradients should fade colors without also interpolating their angles.
    if (state.transitioning && !state.previous.m_wholeMonitor)
        state.previous.m_angle = state.current.m_angle;

    return state;
}

void CAnimatedDecorationGradient::onWindowMap() {
    m_fadeProgress->resetAllCallbacks();
    m_angleProgress->resetAllCallbacks();

    startAngleAnimation();
}

void CAnimatedDecorationGradient::startAngleAnimation() {
    if (!m_angleProgress || m_current.m_wholeMonitor || !m_angleProgress->enabled())
        return;

    m_angleProgress->setCallbackOnEnd(nullptr);
    m_angleProgress->setValueAndWarp(0.F);
    *m_angleProgress = 1.F;
    m_angleProgress->setCallbackOnEnd([this](WP<CBaseAnimatedVariable> animation) { onAngleAnimationEnd(animation); }, false);
}

void CAnimatedDecorationGradient::onWindowFocus() {
    if (m_angleProgress->isBeingAnimated())
        return;

    startAngleAnimation();
}

void CAnimatedDecorationGradient::onAngleAnimationEnd(WP<CBaseAnimatedVariable> animation) {
    if (!animation || animation->getStyle() != "loop" || !animation->enabled())
        return;

    startAngleAnimation();
}
