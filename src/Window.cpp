#include "Window.hpp"
#include "Compositor.hpp"

CWindow::CWindow() {
    m_vRealPosition.create(AVARTYPE_VECTOR, &g_pConfigManager->getConfigValuePtr("animations:windows_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:windows")->intValue, &g_pConfigManager->getConfigValuePtr("animations:windows_curve")->strValue, (void*) this);
    m_vRealSize.create(AVARTYPE_VECTOR, &g_pConfigManager->getConfigValuePtr("animations:windows_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:windows")->intValue, &g_pConfigManager->getConfigValuePtr("animations:windows_curve")->strValue, (void*)this);
    m_cRealBorderColor.create(AVARTYPE_COLOR, &g_pConfigManager->getConfigValuePtr("animations:borders_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:borders")->intValue, &g_pConfigManager->getConfigValuePtr("animations:borders_curve")->strValue, (void*)this);
    m_fAlpha.create(AVARTYPE_FLOAT, &g_pConfigManager->getConfigValuePtr("animations:fadein_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:fadein")->intValue, &g_pConfigManager->getConfigValuePtr("animations:fadein_curve")->strValue, (void*)this);
}

CWindow::~CWindow() {
    if (g_pCompositor->isWindowActive(this)) {
        g_pCompositor->m_pLastFocus = nullptr;
        g_pCompositor->m_pLastWindow = nullptr;
    }
}