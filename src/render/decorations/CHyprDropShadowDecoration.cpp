#include "CHyprDropShadowDecoration.hpp"

#include "../../Compositor.hpp"

CHyprDropShadowDecoration::CHyprDropShadowDecoration(CWindow* pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;
}

CHyprDropShadowDecoration::~CHyprDropShadowDecoration() {}

eDecorationType CHyprDropShadowDecoration::getDecorationType() {
    return DECORATION_SHADOW;
}

SDecorationPositioningInfo CHyprDropShadowDecoration::getPositioningInfo() {
    SDecorationPositioningInfo info;
    info.policy         = DECORATION_POSITION_ABSOLUTE;
    info.desiredExtents = m_seExtents;
    info.edges          = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;

    m_seReportedExtents = m_seExtents;
    return info;
}

void CHyprDropShadowDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    updateWindow(m_pWindow);
}

uint64_t CHyprDropShadowDecoration::getDecorationFlags() {
    return DECORATION_NON_SOLID;
}

std::string CHyprDropShadowDecoration::getDisplayName() {
    return "Drop Shadow";
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
    m_vLastWindowPos  = m_pWindow->m_vRealPosition.vec();
    m_vLastWindowSize = m_pWindow->m_vRealSize.vec();

    m_bLastWindowBox          = {m_vLastWindowPos.x, m_vLastWindowPos.y, m_vLastWindowSize.x, m_vLastWindowSize.y};
    m_bLastWindowBoxWithDecos = g_pDecorationPositioner->getBoxWithIncludedDecos(pWindow);
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

    const auto RADIIBASE = m_pWindow->getCornerRadii();
    const auto RADII = RADIIBASE.topLeft > 0 || RADIIBASE.topRight > 0 || RADIIBASE.bottomRight > 0 || RADIIBASE.bottomLeft > 0 ? RADIIBASE + m_pWindow->getRealBorderSize() : 0;
    const auto PWORKSPACE      = g_pCompositor->getWorkspaceByID(m_pWindow->m_iWorkspaceID);
    const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    // draw the shadow
    CBox fullBox = m_bLastWindowBoxWithDecos;
    fullBox.translate(-pMonitor->vecPosition + WORKSPACEOFFSET);
    fullBox.x -= *PSHADOWSIZE;
    fullBox.y -= *PSHADOWSIZE;
    fullBox.w += 2 * *PSHADOWSIZE;
    fullBox.h += 2 * *PSHADOWSIZE;

    const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.f, 1.f);

    // scale the box in relation to the center of the box
    fullBox.scaleFromCenter(SHADOWSCALE).translate(*PSHADOWOFFSET);

    m_vLastWindowPos += WORKSPACEOFFSET;
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
        CBox withDecos = m_bLastWindowBoxWithDecos;

        // get window box
        windowBox.translate(-pMonitor->vecPosition + WORKSPACEOFFSET);
        withDecos.translate(-pMonitor->vecPosition + WORKSPACEOFFSET);

        auto scaledExtentss = withDecos.extentsFrom(windowBox);
        scaledExtentss      = scaledExtentss * pMonitor->scale;
        scaledExtentss      = scaledExtentss.round();

        // add extents
        windowBox.scale(pMonitor->scale).round().addExtents(scaledExtentss);

        if (windowBox.width < 1 || windowBox.height < 1)
            return; // prevent assert failed

        CRegion saveDamage = g_pHyprOpenGL->m_RenderData.damage;

        g_pHyprOpenGL->m_RenderData.damage = fullBox;

        // TODO: check if CBox has the right params
        CBox substract = windowBox.copy();
        substract.x -= -std::min(RADII.topLeft, RADII.bottomLeft) * pMonitor->scale;
        substract.y -= -std::min(RADII.topRight, RADII.bottomRight) * pMonitor->scale;
        substract.w += -(std::min(RADII.topLeft, RADII.bottomLeft) + std::min(RADII.topRight, RADII.bottomRight)) * pMonitor->scale;
        substract.h += -(std::min(RADII.topLeft, RADII.topRight) + std::min(RADII.bottomLeft, RADII.bottomRight)) * pMonitor->scale;

        g_pHyprOpenGL->m_RenderData.damage.subtract(substract).intersect(saveDamage);

        alphaFB.bind();

        // build the matte
        // 10-bit formats have dogshit alpha channels, so we have to use the matte to its fullest.
        // first, clear region of interest with black (fully transparent)
        g_pHyprOpenGL->renderRect(&fullBox, CColor(0, 0, 0, 1), 0);

        // render white shadow with the alpha of the shadow color (otherwise we clear with alpha later and shit it to 2 bit)
        g_pHyprOpenGL->renderRoundedShadow(&fullBox, RADII * pMonitor->scale, *PSHADOWSIZE * pMonitor->scale, CColor(1, 1, 1, m_pWindow->m_cRealShadowColor.col().a), a);

        // render black window box ("clip")
        g_pHyprOpenGL->renderRect(&windowBox, CColor(0, 0, 0, 1.0), RADII * pMonitor->scale);

        alphaSwapFB.bind();

        // alpha swap just has the shadow color. It will be the "texture" to render.
        g_pHyprOpenGL->renderRect(&fullBox, m_pWindow->m_cRealShadowColor.col().stripA(), 0);

        LASTFB->bind();

        CBox monbox = {0, 0, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y};
        g_pHyprOpenGL->setMonitorTransformEnabled(false);
        g_pHyprOpenGL->renderTextureMatte(alphaSwapFB.m_cTex, &monbox, alphaFB);
        g_pHyprOpenGL->setMonitorTransformEnabled(true);

        g_pHyprOpenGL->m_RenderData.damage = saveDamage;
    } else {
        g_pHyprOpenGL->renderRoundedShadow(&fullBox, RADII * pMonitor->scale, *PSHADOWSIZE * pMonitor->scale, m_pWindow->m_cRealShadowColor.col(), a);
    }

    if (m_seExtents != m_seReportedExtents)
        g_pDecorationPositioner->repositionDeco(this);
}

eDecorationLayer CHyprDropShadowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_BOTTOM;
}
