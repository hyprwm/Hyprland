#include "WindowFadeout.hpp"
#include "WindowState.hpp"
#include "../view/Window.hpp"
#include "../Workspace.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../config/shared/animation/AnimationTree.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../render/Framebuffer.hpp"
#include "../../render/Renderer.hpp"

#include <algorithm>
#include <cmath>

using namespace Desktop;
using namespace Desktop::View;

static int windowZIndex(PHLWINDOW window) {
    const auto& WINDOWS = windowState()->windows();
    const auto  IT      = std::ranges::find(WINDOWS, window);
    return IT == WINDOWS.end() ? 0 : std::distance(WINDOWS.begin(), IT);
}

static bool shouldBlurWindow(PHLWINDOW window) {
    static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    if (!*PBLUR)
        return false;

    if (window->m_ruleApplicator->noBlur().valueOrDefault() || window->m_ruleApplicator->RGBX().valueOrDefault() || window->opaque())
        return false;

    auto surface = window->wlSurface();
    if (surface && surface->m_hasBackgroundEffect)
        return !surface->m_blurRegion.empty();

    return true;
}

static void damageFadeoutMonitor(PHLMONITORREF monitor) {
    if (const auto MONITOR = monitor.lock(); MONITOR)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

template <typename T>
static void damageWeakFadeout(WP<T> fadeout) {
    if (const auto FADEOUT = fadeout.lock(); FADEOUT)
        damageFadeoutMonitor(FADEOUT->monitor());
}

SP<CWindowFadeout> CWindowFadeout::create(PHLWINDOW window, SP<Render::IFramebuffer> snapshot, float sourceAlpha) {
    if (!window || !snapshot)
        return nullptr;

    const auto MONITOR = window->m_monitor.lock();
    if (!MONITOR)
        return nullptr;

    auto fadeout              = SP<CWindowFadeout>(new CWindowFadeout());
    fadeout->m_monitor        = MONITOR;
    fadeout->m_workspace      = window->m_workspace;
    fadeout->m_framebuffer    = snapshot;
    fadeout->m_zIndex         = windowZIndex(window);
    fadeout->m_sourcePos      = window->m_realPosition->value() - MONITOR->m_position;
    fadeout->m_sourceSize     = window->m_realSize->value();
    const bool OVERFULLSCREEN = window->m_isFloating && window->shouldRenderOverFullscreen() && window->m_workspace && window->m_workspace->m_hasFullscreenWindow;
    fadeout->m_plane          = !window->m_isFloating ? FADEOUT_PLANE_WINDOW_TILED : (OVERFULLSCREEN ? FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN : FADEOUT_PLANE_WINDOW_FLOATING);
    fadeout->m_rounding       = window->rounding();
    fadeout->m_roundingPower  = window->roundingPower();
    fadeout->m_blur           = shouldBlurWindow(window);
    fadeout->m_blurXray       = window->m_ruleApplicator->xray().valueOr(false);

    static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");
    if (*PDIMAROUND && window->m_ruleApplicator->dimAround().valueOrDefault())
        fadeout->m_effects.dimAroundAlpha = *PDIMAROUND;

    const auto ANIMCTX = window->m_animationController.animateOut();

    Animation::mgr()->createAnimation(ANIMCTX.pos.from, fadeout->m_realPosition, Config::animationTree()->getAnimationPropertyConfig("windowsOut"), AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(ANIMCTX.size.from, fadeout->m_realSize, Config::animationTree()->getAnimationPropertyConfig("windowsOut"), AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(sourceAlpha, fadeout->m_alpha, Config::animationTree()->getAnimationPropertyConfig("fadeOut"), AVARDAMAGE_NONE);

    const WP<CWindowFadeout> WEAK = fadeout;
    fadeout->m_realPosition->setUpdateCallback([WEAK](auto) { damageWeakFadeout(WEAK); });
    fadeout->m_realSize->setUpdateCallback([WEAK](auto) { damageWeakFadeout(WEAK); });
    fadeout->m_alpha->setUpdateCallback([WEAK](auto) { damageWeakFadeout(WEAK); });

    fadeout->m_realPosition->setValueAndWarp(ANIMCTX.pos.from);
    fadeout->m_realSize->setValueAndWarp(ANIMCTX.size.from);
    fadeout->m_alpha->setValueAndWarp(sourceAlpha);

    *fadeout->m_realPosition = ANIMCTX.pos.to;
    *fadeout->m_realSize     = ANIMCTX.size.to;
    *fadeout->m_alpha        = ANIMCTX.alpha.to;

    if (window->m_X11DoesntWantBorders) {
        fadeout->m_realPosition->warp();
        fadeout->m_realSize->warp();
    }

    return fadeout;
}

PHLMONITORREF CWindowFadeout::monitor() const {
    return m_monitor;
}

eFadeoutPlane CWindowFadeout::plane() const {
    return m_plane;
}

int CWindowFadeout::zIndex() const {
    return m_zIndex;
}

CBox CWindowFadeout::renderBox() const {
    const auto MONITOR = m_monitor.lock();
    if (!MONITOR || m_sourceSize.x == 0 || m_sourceSize.y == 0)
        return {};

    const Vector2D SCALE = {m_realSize->value().x / m_sourceSize.x, m_realSize->value().y / m_sourceSize.y};

    return {((m_realPosition->value().x - MONITOR->m_position.x) * MONITOR->m_scale) - ((m_sourcePos.x * MONITOR->m_scale) * SCALE.x),
            ((m_realPosition->value().y - MONITOR->m_position.y) * MONITOR->m_scale) - ((m_sourcePos.y * MONITOR->m_scale) * SCALE.y), MONITOR->m_transformedSize.x * SCALE.x,
            MONITOR->m_transformedSize.y * SCALE.y};
}

float CWindowFadeout::alpha() const {
    return m_alpha->value();
}

bool CWindowFadeout::done() const {
    return m_alpha->value() == 0.F && !m_alpha->isBeingAnimated() && !m_realPosition->isBeingAnimated() && !m_realSize->isBeingAnimated();
}

SFadeoutRenderEffects CWindowFadeout::effects() const {
    auto effects = m_effects;
    effects.dimAroundAlpha *= alpha();

    const auto MONITOR = m_monitor.lock();
    if (m_blur && MONITOR) {
        effects.preBlur = SFadeoutPreBlur{
            .box           = CBox{m_realPosition->value(), m_realSize->value()}.translate(-MONITOR->m_position).scale(MONITOR->m_scale).round(),
            .round         = m_rounding,
            .roundingPower = m_roundingPower,
            .xray          = m_blurXray,
            .alpha         = std::sqrt(std::max(alpha(), 0.F)),
        };
    }

    return effects;
}
