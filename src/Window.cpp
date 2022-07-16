#include "Window.hpp"
#include "Compositor.hpp"
#include "render/decorations/CHyprDropShadowDecoration.hpp"

CWindow::CWindow() {
    m_vRealPosition.create(AVARTYPE_VECTOR, &g_pConfigManager->getConfigValuePtr("animations:windows_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:windows")->intValue, &g_pConfigManager->getConfigValuePtr("animations:windows_curve")->strValue, (void*) this, AVARDAMAGE_ENTIRE);
    m_vRealSize.create(AVARTYPE_VECTOR, &g_pConfigManager->getConfigValuePtr("animations:windows_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:windows")->intValue, &g_pConfigManager->getConfigValuePtr("animations:windows_curve")->strValue, (void*)this, AVARDAMAGE_ENTIRE);
    m_cRealBorderColor.create(AVARTYPE_COLOR, &g_pConfigManager->getConfigValuePtr("animations:borders_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:borders")->intValue, &g_pConfigManager->getConfigValuePtr("animations:borders_curve")->strValue, (void*)this, AVARDAMAGE_BORDER);
    m_fAlpha.create(AVARTYPE_FLOAT, &g_pConfigManager->getConfigValuePtr("animations:fadein_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:fadein")->intValue, &g_pConfigManager->getConfigValuePtr("animations:fadein_curve")->strValue, (void*)this, AVARDAMAGE_ENTIRE);
    m_fActiveInactiveAlpha.create(AVARTYPE_FLOAT, &g_pConfigManager->getConfigValuePtr("animations:fadein_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:fadein")->intValue, &g_pConfigManager->getConfigValuePtr("animations:fadein_curve")->strValue, (void*)this, AVARDAMAGE_ENTIRE);
    m_cRealShadowColor.create(AVARTYPE_COLOR, &g_pConfigManager->getConfigValuePtr("animations:borders_speed")->floatValue, &g_pConfigManager->getConfigValuePtr("animations:borders")->intValue, &g_pConfigManager->getConfigValuePtr("animations:borders_curve")->strValue, (void*)this, AVARDAMAGE_SHADOW);

    m_dWindowDecorations.emplace_back(std::make_unique<CHyprDropShadowDecoration>(this)); // put the shadow so it's the first deco (has to be rendered first)
}

CWindow::~CWindow() {
    if (g_pCompositor->isWindowActive(this)) {
        g_pCompositor->m_pLastFocus = nullptr;
        g_pCompositor->m_pLastWindow = nullptr;
    }
}

wlr_box CWindow::getFullWindowBoundingBox() {
    static auto* const PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;

    SWindowDecorationExtents maxExtents = {{*PBORDERSIZE + 1, *PBORDERSIZE + 1}, {*PBORDERSIZE + 1, *PBORDERSIZE + 1}};

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

wlr_box CWindow::getWindowIdealBoundingBoxIgnoreReserved() {

    const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

    auto POS = m_vPosition;
    auto SIZE = m_vSize;

    if (DELTALESSTHAN(POS.y - PMONITOR->vecPosition.y, PMONITOR->vecReservedTopLeft.y, 1)) {
        POS.y = PMONITOR->vecPosition.y;
        SIZE.y += PMONITOR->vecReservedTopLeft.y;
    }
    if (DELTALESSTHAN(POS.x - PMONITOR->vecPosition.x, PMONITOR->vecReservedTopLeft.x, 1)) {
        POS.x = PMONITOR->vecPosition.x;
        SIZE.x += PMONITOR->vecReservedTopLeft.x;
    }
    if (DELTALESSTHAN(POS.x + SIZE.x - PMONITOR->vecPosition.x, PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x, 1)) {
        SIZE.x += PMONITOR->vecReservedBottomRight.x;
    }
    if (DELTALESSTHAN(POS.y + SIZE.y - PMONITOR->vecPosition.y, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y, 1)) {
        SIZE.y += PMONITOR->vecReservedBottomRight.y;
    }

    return wlr_box{(int)POS.x, (int)POS.y, (int)SIZE.x, (int)SIZE.y};
}

void CWindow::updateWindowDecos() {
    for (auto& wd : m_dWindowDecorations)
        wd->updateWindow(this);

    for (auto& wd : m_vDecosToRemove) {
        for (auto it = m_dWindowDecorations.begin(); it != m_dWindowDecorations.end(); it++) {
            if (it->get() == wd) {
                it = m_dWindowDecorations.erase(it);
                if (it == m_dWindowDecorations.end())
                    break;
            }
        }
    }

    m_vDecosToRemove.clear();
}

pid_t CWindow::getPID() {
    pid_t PID = -1;
    if (!m_bIsX11) {
        const auto CLIENT = wl_resource_get_client(m_uSurface.xdg->resource);
        wl_client_get_credentials(CLIENT, &PID, nullptr, nullptr);
    } else {
        PID = m_uSurface.xwayland->pid;
    }

    return PID;
}

IHyprWindowDecoration* CWindow::getDecorationByType(eDecorationType type) {
    for (auto& wd : m_dWindowDecorations) {
        if (wd->getDecorationType() == type)
            return wd.get();
    }

    return nullptr;
}