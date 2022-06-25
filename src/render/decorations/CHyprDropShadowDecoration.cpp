#include "CHyprDropShadowDecoration.hpp"

#include "../../Compositor.hpp"

CHyprDropShadowDecoration::CHyprDropShadowDecoration(CWindow* pWindow) {
    m_pWindow = pWindow;
}

CHyprDropShadowDecoration::~CHyprDropShadowDecoration() {
    updateWindow(m_pWindow);
}

SWindowDecorationExtents CHyprDropShadowDecoration::getWindowDecorationExtents() {
    static auto *const PSHADOWS = &g_pConfigManager->getConfigValuePtr("decoration:drop_shadow")->intValue;

    if (*PSHADOWS != 1)
        return {{}, {}};

    return m_seExtents;
}

eDecorationType CHyprDropShadowDecoration::getDecorationType() {
    return DECORATION_SHADOW;
}

void CHyprDropShadowDecoration::damageEntire() {
    static auto *const PSHADOWS = &g_pConfigManager->getConfigValuePtr("decoration:drop_shadow")->intValue;

    if (*PSHADOWS != 1)
        return; // disabled

    wlr_box dm = {m_vLastWindowPos.x - m_seExtents.topLeft.x, m_vLastWindowPos.y - m_seExtents.topLeft.y, m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x, m_vLastWindowSize.y + m_seExtents.topLeft.y + m_seExtents.bottomRight.y};
    g_pHyprRenderer->damageBox(&dm);
}

void CHyprDropShadowDecoration::updateWindow(CWindow* pWindow) {
    damageEntire();
}

void CHyprDropShadowDecoration::draw(SMonitor* pMonitor) {

    if (!g_pCompositor->windowValidMapped(m_pWindow))
        return;

    static auto *const PSHADOWS = &g_pConfigManager->getConfigValuePtr("decoration:drop_shadow")->intValue;
    static auto *const PSHADOWSIZE = &g_pConfigManager->getConfigValuePtr("decoration:shadow_range")->intValue;
    static auto *const PROUNDING = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;

    if (*PSHADOWS != 1)
        return; // disabled

    // update the extents if needed
    if (*PSHADOWSIZE != m_seExtents.topLeft.x) 
        m_seExtents = {{*PSHADOWSIZE + 2, *PSHADOWSIZE + 2}, {*PSHADOWSIZE + 2, *PSHADOWSIZE + 2}};

    m_vLastWindowPos = m_pWindow->m_vRealPosition.vec();
    m_vLastWindowSize = m_pWindow->m_vRealSize.vec();

    // draw the shadow
    wlr_box fullBox = {m_vLastWindowPos.x - m_seExtents.topLeft.x + 2, m_vLastWindowPos.y - m_seExtents.topLeft.y + 2, m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x - 4, m_vLastWindowSize.y + m_seExtents.topLeft.y + m_seExtents.bottomRight.y - 4};
    
    fullBox.x -= pMonitor->vecPosition.x;
    fullBox.y -= pMonitor->vecPosition.y;
    
    g_pHyprOpenGL->renderRoundedShadow(&fullBox, *PROUNDING, *PSHADOWSIZE);
}
