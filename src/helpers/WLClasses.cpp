#include "WLClasses.hpp"
#include "../config/ConfigManager.hpp"

SLayerSurface::SLayerSurface() {
    alpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), nullptr, AVARDAMAGE_ENTIRE);
    alpha.m_pLayer = this;
    alpha.registerVar();
}

void SLayerSurface::applyRules() {
    noAnimations = false;
    forceBlur    = false;
    ignoreZero   = false;

    for (auto& rule : g_pConfigManager->getMatchingRules(this)) {
        if (rule.rule == "noanim")
            noAnimations = true;
        else if (rule.rule == "blur")
            forceBlur = true;
        else if (rule.rule == "ignorezero")
            ignoreZero = true;
    }
}