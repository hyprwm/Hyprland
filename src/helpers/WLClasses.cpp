#include "WLClasses.hpp"
#include "../config/ConfigManager.hpp"

SLayerSurface::SLayerSurface() {
    alpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), nullptr, AVARDAMAGE_ENTIRE);
    alpha.m_pLayer = this;
    alpha.registerVar();
}

void SLayerSurface::applyRules() {
    noAnimations     = false;
    forceBlur        = false;
    ignoreAlpha      = false;
    ignoreAlphaValue = 0.f;

    for (auto& rule : g_pConfigManager->getMatchingRules(this)) {
        if (rule.rule == "noanim")
            noAnimations = true;
        else if (rule.rule == "blur")
            forceBlur = true;
        else if (rule.rule.find("ignorealpha") == 0 || rule.rule.find("ignorezero") == 0) {
            const auto  FIRST_SPACE_POS = rule.rule.find_first_of(' ');
            std::string alphaValue      = "";
            if (FIRST_SPACE_POS != std::string::npos)
                alphaValue = rule.rule.substr(FIRST_SPACE_POS + 1);

            try {
                ignoreAlpha = true;
                if (!alphaValue.empty())
                    ignoreAlphaValue = std::stof(alphaValue);
            } catch (...) { Debug::log(ERR, "Invalid value passed to ignoreAlpha"); }
        }
    }
}