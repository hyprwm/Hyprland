#include "Window.hpp"
#include "Compositor.hpp"

CWindow::CWindow() {
    m_vRealPosition.create(AVARTYPE_VECTOR, &g_pConfigManager->getConfigValuePtr("animations:windows_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:windows")->intValue, &g_pConfigManager->getConfigValuePtr("animations:windows_curve")->strValue, (void*) this, AVARDAMAGE_ENTIRE);
    m_vRealSize.create(AVARTYPE_VECTOR, &g_pConfigManager->getConfigValuePtr("animations:windows_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:windows")->intValue, &g_pConfigManager->getConfigValuePtr("animations:windows_curve")->strValue, (void*)this, AVARDAMAGE_ENTIRE);
    m_cRealBorderColor.create(AVARTYPE_COLOR, &g_pConfigManager->getConfigValuePtr("animations:borders_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:borders")->intValue, &g_pConfigManager->getConfigValuePtr("animations:borders_curve")->strValue, (void*)this, AVARDAMAGE_BORDER);
    m_fAlpha.create(AVARTYPE_FLOAT, &g_pConfigManager->getConfigValuePtr("animations:fadein_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:fadein")->intValue, &g_pConfigManager->getConfigValuePtr("animations:fadein_curve")->strValue, (void*)this, AVARDAMAGE_ENTIRE);
}

CWindow::~CWindow() {
    if (g_pCompositor->isWindowActive(this)) {
        g_pCompositor->m_pLastFocus = nullptr;
        g_pCompositor->m_pLastWindow = nullptr;
    }
}

wlr_box CWindow::getFullWindowBoundingBox() {

    SWindowDecorationExtents maxExtents;

    for (auto& wd : m_dWindowDecorations) {

        const auto EXTENTS = wd->getWindowDecorationExtents();

        if (EXTENTS.topLeft.x > maxExtents.topLeft.x)
            maxExtents.topLeft.x = EXTENTS.topLeft.x;

        if (EXTENTS.topLeft.y > maxExtents.topLeft.y)
            maxExtents.topLeft.y = EXTENTS.topLeft.y;

        if (EXTENTS.bottomRight.x > maxExtents.bottomRight.x)
            maxExtents.bottomRight.x = EXTENTS.bottomRight.x;

        if (EXTENTS.bottomRight.y > maxExtents.bottomRight.y)
            maxExtents.bottomRight.y = EXTENTS.bottomRight.y;
    }

    // Add extents to the real base BB and return
    wlr_box finalBox = {m_vRealPosition.vec().x - maxExtents.topLeft.x,
                        m_vRealPosition.vec().y - maxExtents.topLeft.y,
                        m_vRealSize.vec().x + maxExtents.topLeft.x + maxExtents.bottomRight.x,
                        m_vRealSize.vec().y + maxExtents.topLeft.y + maxExtents.bottomRight.y};
    
    return finalBox;
}