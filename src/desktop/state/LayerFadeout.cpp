#include "LayerFadeout.hpp"
#include "../view/LayerSurface.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../config/shared/animation/AnimationTree.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../render/Framebuffer.hpp"
#include "../../render/Renderer.hpp"

#include <algorithm>
#include <cmath>

using namespace Desktop;
using namespace Desktop::View;

static int layerZIndex(PHLLS layer) {
    const auto MONITOR = layer ? layer->m_monitor.lock() : nullptr;
    if (!MONITOR || layer->m_layer >= MONITOR->m_layerSurfaceLayers.size())
        return 0;

    const auto& LAYERS = MONITOR->m_layerSurfaceLayers[layer->m_layer];
    const auto  IT     = std::ranges::find_if(LAYERS, [&](const auto& other) { return other.lock() == layer; });
    return IT == LAYERS.end() ? 0 : std::distance(LAYERS.begin(), IT);
}

static bool shouldBlurLayer(PHLLS layer) {
    static auto PBLUR = CConfigValue<Config::INTEGER>("decoration:blur:enabled");
    if (!*PBLUR)
        return false;

    auto surface = layer->wlSurface();
    if (surface && surface->m_hasBackgroundEffect)
        return !surface->m_blurRegion.empty();

    return layer->m_ruleApplicator->blur().valueOrDefault();
}

static void damageFadeoutMonitor(PHLMONITORREF monitor) {
    if (const auto MONITOR = monitor.lock(); MONITOR)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

SP<CLayerFadeout> CLayerFadeout::create(PHLLS layer, SP<Render::IFramebuffer> snapshot, float sourceAlpha) {
    if (!layer || !snapshot)
        return nullptr;

    const auto MONITOR = layer->m_monitor.lock();
    if (!MONITOR)
        return nullptr;

    auto fadeout           = SP<CLayerFadeout>(new CLayerFadeout());
    fadeout->m_monitor     = MONITOR;
    fadeout->m_framebuffer = snapshot;
    fadeout->m_geometry    = layer->m_geometry;
    fadeout->m_zIndex      = layerZIndex(layer);

    static auto PDIMAROUND = CConfigValue<Config::FLOAT>("decoration:dim_around");
    if (*PDIMAROUND && layer->m_ruleApplicator->dimAround().valueOrDefault())
        fadeout->m_effects.dimAroundAlpha = *PDIMAROUND;

    fadeout->m_effects.textureBlur.enabled = shouldBlurLayer(layer);
    if (fadeout->m_effects.textureBlur.enabled)
        fadeout->m_effects.textureBlur.ignoreAlpha = layer->m_ruleApplicator->ignoreAlpha().valueOr(0.01F);

    switch (layer->m_layer) {
        case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND: fadeout->m_plane = FADEOUT_PLANE_LAYER_BACKGROUND; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM: fadeout->m_plane = FADEOUT_PLANE_LAYER_BOTTOM; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_TOP: fadeout->m_plane = FADEOUT_PLANE_LAYER_TOP; break;
        case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY: fadeout->m_plane = FADEOUT_PLANE_LAYER_OVERLAY; break;
        default: fadeout->m_plane = FADEOUT_PLANE_LAYER_TOP; break;
    }

    fadeout->m_marksBlurDirty = layer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || layer->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;

    const auto ANIMCTX = layer->m_animationController.animateOut();

    Animation::mgr()->createAnimation(ANIMCTX.pos.from, fadeout->m_realPosition, Config::animationTree()->getAnimationPropertyConfig("layersOut"), AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(ANIMCTX.size.from, fadeout->m_realSize, Config::animationTree()->getAnimationPropertyConfig("layersOut"), AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(sourceAlpha, fadeout->m_alpha, Config::animationTree()->getAnimationPropertyConfig("fadeLayersOut"), AVARDAMAGE_NONE);

    const WP<CLayerFadeout> WEAK   = fadeout;
    const auto              DAMAGE = [WEAK](auto) {
        if (const auto FADEOUT = WEAK.lock(); FADEOUT) {
            damageFadeoutMonitor(FADEOUT->m_monitor);
            if (const auto MONITOR = FADEOUT->m_monitor.lock(); MONITOR && FADEOUT->m_marksBlurDirty)
                MONITOR->m_blurFBDirty = true;
        }
    };
    fadeout->m_realPosition->setUpdateCallback(DAMAGE);
    fadeout->m_realSize->setUpdateCallback(DAMAGE);
    fadeout->m_alpha->setUpdateCallback(DAMAGE);

    fadeout->m_realPosition->setValueAndWarp(ANIMCTX.pos.from);
    fadeout->m_realSize->setValueAndWarp(ANIMCTX.size.from);
    fadeout->m_alpha->setValueAndWarp(sourceAlpha);

    *fadeout->m_realPosition = ANIMCTX.pos.to;
    *fadeout->m_realSize     = ANIMCTX.size.to;
    *fadeout->m_alpha        = ANIMCTX.alpha.to;

    return fadeout;
}

PHLMONITORREF CLayerFadeout::monitor() const {
    return m_monitor;
}

eFadeoutPlane CLayerFadeout::plane() const {
    return m_plane;
}

int CLayerFadeout::zIndex() const {
    return m_zIndex;
}

CBox CLayerFadeout::renderBox() const {
    const auto MONITOR = m_monitor.lock();
    if (!MONITOR || m_geometry.w == 0 || m_geometry.h == 0)
        return {};

    const Vector2D SCALE = {m_realSize->value().x / m_geometry.w, m_realSize->value().y / m_geometry.h};

    return {((m_realPosition->value().x - MONITOR->m_position.x) * MONITOR->m_scale) - (((m_geometry.x - MONITOR->m_position.x) * MONITOR->m_scale) * SCALE.x),
            ((m_realPosition->value().y - MONITOR->m_position.y) * MONITOR->m_scale) - (((m_geometry.y - MONITOR->m_position.y) * MONITOR->m_scale) * SCALE.y),
            MONITOR->m_transformedSize.x * SCALE.x, MONITOR->m_transformedSize.y * SCALE.y};
}

float CLayerFadeout::alpha() const {
    return m_alpha->value();
}

bool CLayerFadeout::done() const {
    return m_alpha->value() == 0.F && !m_alpha->isBeingAnimated() && !m_realPosition->isBeingAnimated() && !m_realSize->isBeingAnimated();
}

SFadeoutRenderEffects CLayerFadeout::effects() const {
    auto effects = m_effects;
    effects.dimAroundAlpha *= alpha();

    if (effects.textureBlur.enabled)
        effects.textureBlur.alpha = std::sqrt(std::max(alpha(), 0.F));

    return effects;
}
