#include "PopupFadeout.hpp"
#include "../view/LayerSurface.hpp"
#include "../view/Popup.hpp"
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

static bool shouldBlurPopup() {
    static CConfigValue PBLURPOPUPS = CConfigValue<Config::INTEGER>("decoration:blur:popups");
    static CConfigValue PBLUR       = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

    return *PBLURPOPUPS && *PBLUR;
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

SP<CPopupFadeout> CPopupFadeout::create(SP<CPopup> popup, SP<Render::IFramebuffer> snapshot, float sourceAlpha) {
    if (!popup || !snapshot)
        return nullptr;

    const auto MONITOR = popup->getMonitor();
    if (!MONITOR)
        return nullptr;

    auto fadeout           = SP<CPopupFadeout>(new CPopupFadeout());
    fadeout->m_monitor     = MONITOR;
    fadeout->m_framebuffer = snapshot;

    static CConfigValue PBLURIGNOREA = CConfigValue<Config::FLOAT>("decoration:blur:popups_ignorealpha");
    if (shouldBlurPopup()) {
        fadeout->m_effects.textureBlur.enabled               = true;
        fadeout->m_effects.textureBlur.blockBlurOptimization = true;

        if (const auto PLAYER = popup->layerOwner(); PLAYER && PLAYER->m_ruleApplicator->ignoreAlpha().hasValue())
            fadeout->m_effects.textureBlur.ignoreAlpha = std::max(PLAYER->m_ruleApplicator->ignoreAlpha().valueOrDefault(), 0.01F);
        else
            fadeout->m_effects.textureBlur.ignoreAlpha = std::max(*PBLURIGNOREA, 0.01F);
    }

    const auto ANIMCTX = popup->m_animationController.animateOut();

    Animation::mgr()->createAnimation(ANIMCTX.pos.from, fadeout->m_realPosition, Config::animationTree()->getAnimationPropertyConfig("fadePopupsOut"), AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(ANIMCTX.size.from, fadeout->m_realSize, Config::animationTree()->getAnimationPropertyConfig("fadePopupsOut"), AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(sourceAlpha, fadeout->m_alpha, Config::animationTree()->getAnimationPropertyConfig("fadePopupsOut"), AVARDAMAGE_NONE);
    const WP<CPopupFadeout> WEAK = fadeout;
    fadeout->m_alpha->setUpdateCallback([WEAK](auto) { damageWeakFadeout(WEAK); });
    fadeout->m_alpha->setValueAndWarp(sourceAlpha);
    *fadeout->m_alpha = ANIMCTX.alpha.to;

    return fadeout;
}

PHLMONITORREF CPopupFadeout::monitor() const {
    return m_monitor;
}

eFadeoutPlane CPopupFadeout::plane() const {
    return FADEOUT_PLANE_POPUP;
}

int CPopupFadeout::zIndex() const {
    return m_zIndex;
}

CBox CPopupFadeout::renderBox() const {
    const auto MONITOR = m_monitor.lock();
    return MONITOR ? CBox{{}, MONITOR->m_transformedSize} : CBox{};
}

float CPopupFadeout::alpha() const {
    return m_alpha->value();
}

bool CPopupFadeout::done() const {
    return m_alpha->value() == 0.F && !m_alpha->isBeingAnimated();
}

SFadeoutRenderEffects CPopupFadeout::effects() const {
    auto effects = m_effects;

    if (effects.textureBlur.enabled)
        effects.textureBlur.alpha = std::sqrt(std::max(alpha(), 0.F));

    return effects;
}
