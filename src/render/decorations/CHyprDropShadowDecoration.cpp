#include "CHyprDropShadowDecoration.hpp"

#include "../../Compositor.hpp"

CHyprDropShadowDecoration::CHyprDropShadowDecoration(CWindow* pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;
}

CHyprDropShadowDecoration::~CHyprDropShadowDecoration() {
    updateWindow(m_pWindow);
}

SWindowDecorationExtents CHyprDropShadowDecoration::getWindowDecorationExtents() {
    static auto* const PSHADOWS = &g_pConfigManager->getConfigValuePtr("decoration:drop_shadow")->intValue;

    if (*PSHADOWS != 1)
        return {{}, {}};

    return m_seExtents;
}

eDecorationType CHyprDropShadowDecoration::getDecorationType() {
    return DECORATION_SHADOW;
}

void CHyprDropShadowDecoration::damageEntire() {
    static auto* const PSHADOWS = &g_pConfigManager->getConfigValuePtr("decoration:drop_shadow")->intValue;

    if (*PSHADOWS != 1)
        return; // disabled

    CBox dm = {m_vLastWindowPos.x - m_seExtents.topLeft.x, m_vLastWindowPos.y - m_seExtents.topLeft.y, m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x,
               m_vLastWindowSize.y + m_seExtents.topLeft.y + m_seExtents.bottomRight.y};
    g_pHyprRenderer->damageBox(&dm);
}

void CHyprDropShadowDecoration::updateWindow(CWindow* pWindow) {

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    if (pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET != m_vLastWindowPos || pWindow->m_vRealSize.vec() != m_vLastWindowSize) {
        m_vLastWindowPos  = pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET;
        m_vLastWindowSize = pWindow->m_vRealSize.vec();

        damageEntire();

        const auto BORDER = m_pWindow->getRealBorderSize();

        // calculate extents of decos with the DECORATION_PART_OF_MAIN_WINDOW flag
        SWindowDecorationExtents maxExtents;

        for (auto& wd : m_pWindow->m_dWindowDecorations) {
            // conveniently, this will also skip us.
            if (!(wd->getDecorationFlags() & DECORATION_PART_OF_MAIN_WINDOW))
                continue;

            const auto EXTENTS = wd->getWindowDecorationExtents();

            if (maxExtents.topLeft.x < EXTENTS.topLeft.x)
                maxExtents.topLeft.x = EXTENTS.topLeft.x;
            if (maxExtents.topLeft.y < EXTENTS.topLeft.y)
                maxExtents.topLeft.y = EXTENTS.topLeft.y;
            if (maxExtents.bottomRight.x < EXTENTS.bottomRight.x)
                maxExtents.bottomRight.x = EXTENTS.bottomRight.x;
            if (maxExtents.bottomRight.y < EXTENTS.bottomRight.y)
                maxExtents.bottomRight.y = EXTENTS.bottomRight.y;
        }

        m_bLastWindowBox = {m_vLastWindowPos.x, m_vLastWindowPos.y, m_vLastWindowSize.x, m_vLastWindowSize.y};
        m_eLastExtents   = {{maxExtents.topLeft + Vector2D{BORDER, BORDER}}, {maxExtents.bottomRight + Vector2D{BORDER, BORDER}}};
    }
}

void CHyprDropShadowDecoration::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {

    if (!g_pCompositor->windowValidMapped(m_pWindow))
        return;

    if (m_pWindow->m_cRealShadowColor.col() == CColor(0, 0, 0, 0))
        return; // don't draw invisible shadows

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    if (!m_pWindow->m_sSpecialRenderData.shadow)
        return;

    if (m_pWindow->m_sAdditionalConfigData.forceNoShadow)
        return;

    static auto* const PSHADOWS            = &g_pConfigManager->getConfigValuePtr("decoration:drop_shadow")->intValue;
    static auto* const PSHADOWSIZE         = &g_pConfigManager->getConfigValuePtr("decoration:shadow_range")->intValue;
    static auto* const PSHADOWIGNOREWINDOW = &g_pConfigManager->getConfigValuePtr("decoration:shadow_ignore_window")->intValue;
    static auto* const PSHADOWSCALE        = &g_pConfigManager->getConfigValuePtr("decoration:shadow_scale")->floatValue;
    static auto* const PSHADOWOFFSET       = &g_pConfigManager->getConfigValuePtr("decoration:shadow_offset")->vecValue;

    if (*PSHADOWS != 1)
        return; // disabled

    const auto ROUNDING = m_pWindow->rounding() + m_pWindow->getRealBorderSize();

    // draw the shadow
    CBox fullBox = {m_bLastWindowBox.x, m_bLastWindowBox.y, m_bLastWindowBox.width, m_bLastWindowBox.height};
    fullBox.addExtents(m_eLastExtents).translate(-pMonitor->vecPosition);
    fullBox.x -= *PSHADOWSIZE;
    fullBox.y -= *PSHADOWSIZE;
    fullBox.w += 2 * *PSHADOWSIZE;
    fullBox.h += 2 * *PSHADOWSIZE;

    const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.f, 1.f);

    // scale the box in relation to the center of the box
    fullBox.scaleFromCenter(SHADOWSCALE).translate(*PSHADOWOFFSET);

    m_seExtents = {{m_vLastWindowPos.x - fullBox.x - pMonitor->vecPosition.x + 2, m_vLastWindowPos.y - fullBox.y - pMonitor->vecPosition.y + 2},
                   {fullBox.x + fullBox.width + pMonitor->vecPosition.x - m_vLastWindowPos.x - m_vLastWindowSize.x + 2,
                    fullBox.y + fullBox.height + pMonitor->vecPosition.y - m_vLastWindowPos.y - m_vLastWindowSize.y + 2}};

    fullBox.translate(offset);

    if (fullBox.width < 1 || fullBox.height < 1)
        return; // don't draw invisible shadows

    g_pHyprOpenGL->scissor((CBox*)nullptr);

    // we'll take the liberty of using this as it should not be used rn
    CFramebuffer& alphaFB     = g_pHyprOpenGL->m_RenderData.pCurrentMonData->mirrorFB;
    CFramebuffer& alphaSwapFB = g_pHyprOpenGL->m_RenderData.pCurrentMonData->mirrorSwapFB;
    auto*         LASTFB      = g_pHyprOpenGL->m_RenderData.currentFB;

    fullBox.scale(pMonitor->scale).round();

    if (*PSHADOWIGNOREWINDOW) {
        CBox windowBox = m_bLastWindowBox;

        windowBox.translate(-pMonitor->vecPosition).scale(pMonitor->scale);
        windowBox.round();

        windowBox.addExtents(SWindowDecorationExtents{m_eLastExtents * pMonitor->scale}.floor()).round();

        if (windowBox.width < 1 || windowBox.height < 1) {
            return; // prevent assert failed
        }

        alphaFB.bind();
        g_pHyprOpenGL->clear(CColor(0, 0, 0, 0));

        g_pHyprOpenGL->renderRect(&windowBox, CColor(1.0, 1.0, 1.0, 1.0), ROUNDING * pMonitor->scale);

        alphaSwapFB.bind();
        g_pHyprOpenGL->clear(CColor(0, 0, 0, 0));

        g_pHyprOpenGL->renderRoundedShadow(&fullBox, ROUNDING * pMonitor->scale, *PSHADOWSIZE * pMonitor->scale, a);

        LASTFB->bind();

        CBox monbox = {0, 0, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y};
        g_pHyprOpenGL->setMonitorTransformEnabled(false);
        g_pHyprOpenGL->renderTextureMatte(alphaSwapFB.m_cTex, &monbox, alphaFB);
        g_pHyprOpenGL->setMonitorTransformEnabled(true);
    } else {
        g_pHyprOpenGL->renderRoundedShadow(&fullBox, ROUNDING * pMonitor->scale, *PSHADOWSIZE * pMonitor->scale, a);
    }
}

eDecorationLayer CHyprDropShadowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_BOTTOM;
}