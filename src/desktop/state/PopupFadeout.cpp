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

// the snapshot is baked at the popup's global coords and doesn't move, so only its own box needs damage
static void damageWeakFadeout(WP<CPopupFadeout> fadeout) {
    if (const auto FADEOUT = fadeout.lock(); FADEOUT)
        g_pHyprRenderer->damageBox(FADEOUT->damageBox());
}

SP<CPopupFadeout> CPopupFadeout::create(SP<CPopup> popup, SP<Render::IFramebuffer> snapshot, float sourceAlpha) {
    if (!popup || !snapshot)
        return nullptr;

    const auto MONITOR = popup->getMonitor();
    if (!MONITOR)
        return nullptr;

    const auto EXTENDS = popup->wlSurface()->resource()->extends();
    const auto ORIGIN  = popup->coordsGlobal() + EXTENDS.pos();

    auto       fadeout     = SP<CPopupFadeout>(new CPopupFadeout());
    fadeout->m_monitor     = MONITOR;
    fadeout->m_framebuffer = snapshot;
    fadeout->m_damageBox   = CBox{ORIGIN, EXTENDS.size()}.expand(4);
    // take the size from the FB so the snapshot maps 1:1 and doesn't get rescaled mid-fade
    fadeout->m_renderBox = CBox{((ORIGIN - MONITOR->m_position) * MONITOR->m_scale).round(), snapshot->m_size};

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
    return m_renderBox;
}

float CPopupFadeout::alpha() const {
    return m_alpha->value();
}

bool CPopupFadeout::done() const {
    return m_alpha->value() == 0.F && !m_alpha->isBeingAnimated();
}

const CBox& CPopupFadeout::damageBox() const {
    return m_damageBox;
}

SFadeoutRenderEffects CPopupFadeout::effects() const {
    auto effects = m_effects;

    if (effects.textureBlur.enabled)
        effects.textureBlur.alpha = std::sqrt(std::max(alpha(), 0.F));

    return effects;
}
