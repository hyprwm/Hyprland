#include "customDecoration.hpp"
#include <hyprland/src/Window.hpp>
#include <hyprland/src/Compositor.hpp>
#include "globals.hpp"

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

    static auto* const PCOLOR      = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:example:border_color")->intValue;
    static auto* const PROUNDING   = &HyprlandAPI::getConfigValue(PHANDLE, "decoration:rounding")->intValue;
    static auto* const PBORDERSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->intValue;

    const auto         ROUNDING = !m_pWindow->m_sSpecialRenderData.rounding ?
                0 :
                (m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying() == -1 ? *PROUNDING : m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying());

    // draw the border
    CBox fullBox = {(int)(m_vLastWindowPos.x - *PBORDERSIZE), (int)(m_vLastWindowPos.y - *PBORDERSIZE), (int)(m_vLastWindowSize.x + 2.0 * *PBORDERSIZE),
                    (int)(m_vLastWindowSize.y + 2.0 * *PBORDERSIZE)};

    fullBox.x -= pMonitor->vecPosition.x;
    fullBox.y -= pMonitor->vecPosition.y;

    m_seExtents = {{m_vLastWindowPos.x - fullBox.x - pMonitor->vecPosition.x + 2, m_vLastWindowPos.y - fullBox.y - pMonitor->vecPosition.y + 2},
                   {fullBox.x + fullBox.width + pMonitor->vecPosition.x - m_vLastWindowPos.x - m_vLastWindowSize.x + 2,
                    fullBox.y + fullBox.height + pMonitor->vecPosition.y - m_vLastWindowPos.y - m_vLastWindowSize.y + 2}};

    fullBox.x += offset.x;
    fullBox.y += offset.y;

    if (fullBox.width < 1 || fullBox.height < 1)
        return; // don't draw invisible shadows

    g_pHyprOpenGL->scissor((CBox*)nullptr);

    fullBox.scale(pMonitor->scale);
    g_pHyprOpenGL->renderBorder(&fullBox, CColor(*PCOLOR), *PROUNDING * pMonitor->scale + *PBORDERSIZE * 2, a);
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
    CBox dm = {(int)(m_vLastWindowPos.x - m_seExtents.topLeft.x), (int)(m_vLastWindowPos.y - m_seExtents.topLeft.y),
               (int)(m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x), (int)m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}