#include "customDecoration.hpp"
#include "../../src/Window.hpp"
#include "../../src/Compositor.hpp"

CCustomDecoration::CCustomDecoration(CWindow* pWindow) {
    m_pWindow         = pWindow;
    m_vLastWindowPos  = pWindow->m_vRealPosition.vec();
    m_vLastWindowSize = pWindow->m_vRealSize.vec();
}

CCustomDecoration::~CCustomDecoration() {
    damageEntire();
}

SWindowDecorationExtents CCustomDecoration::getWindowDecorationExtents() {
    return m_seExtents;
}

void CCustomDecoration::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    if (!g_pCompositor->windowValidMapped(m_pWindow))
        return;

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    static auto* const PROUNDING   = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;
    static auto* const PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;

    const auto         ROUNDING = !m_pWindow->m_sSpecialRenderData.rounding ?
                0 :
                (m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying() == -1 ? *PROUNDING : m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying());

    // draw the border
    wlr_box fullBox = {m_vLastWindowPos.x - *PBORDERSIZE, m_vLastWindowPos.y - *PBORDERSIZE, m_vLastWindowSize.x + 2.0 * *PBORDERSIZE, m_vLastWindowSize.y + 2.0 * *PBORDERSIZE};

    fullBox.x -= pMonitor->vecPosition.x;
    fullBox.y -= pMonitor->vecPosition.y;

    m_seExtents = {{m_vLastWindowPos.x - fullBox.x - pMonitor->vecPosition.x + 2, m_vLastWindowPos.y - fullBox.y - pMonitor->vecPosition.y + 2},
                   {fullBox.x + fullBox.width + pMonitor->vecPosition.x - m_vLastWindowPos.x - m_vLastWindowSize.x + 2,
                    fullBox.y + fullBox.height + pMonitor->vecPosition.y - m_vLastWindowPos.y - m_vLastWindowSize.y + 2}};

    fullBox.x += offset.x;
    fullBox.y += offset.y;

    if (fullBox.width < 1 || fullBox.height < 1)
        return; // don't draw invisible shadows

    g_pHyprOpenGL->scissor((wlr_box*)nullptr);

    scaleBox(&fullBox, pMonitor->scale);
    g_pHyprOpenGL->renderBorder(&fullBox, CColor(0.f, 0.f, 0.f, a), *PROUNDING * pMonitor->scale + *PBORDERSIZE * 2);
}

eDecorationType CCustomDecoration::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CCustomDecoration::updateWindow(CWindow* pWindow) {

    m_vLastWindowPos  = pWindow->m_vRealPosition.vec();
    m_vLastWindowSize = pWindow->m_vRealSize.vec();

    damageEntire();
}

void CCustomDecoration::damageEntire() {
    wlr_box dm = {m_vLastWindowPos.x - m_seExtents.topLeft.x, m_vLastWindowPos.y - m_seExtents.topLeft.y, m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x,
                  m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}