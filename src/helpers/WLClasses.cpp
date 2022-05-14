#include "WLClasses.hpp"
#include "../config/ConfigManager.hpp"

SLayerSurface::SLayerSurface() {
    alpha.create(AVARTYPE_FLOAT, &g_pConfigManager->getConfigValuePtr("animations:fadein_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:fadein")->intValue, &g_pConfigManager->getConfigValuePtr("animations:fadein_curve")->strValue, nullptr, AVARDAMAGE_ENTIRE);
    alpha.m_pLayer = this;
}