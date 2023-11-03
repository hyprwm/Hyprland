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

    wlr_box dm = {m_vLastWindowPos.x - m_seExtents.topLeft.x, m_vLastWindowPos.y - m_seExtents.topLeft.y, m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x,
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

        // +1 +1 -2 -2 is to avoid artifacts with AA. TODO: figure out a better method. Alpha blending? This same shit will happen on hyprbars.
        m_bLastWindowBox = {(int)(m_vLastWindowPos.x - maxExtents.topLeft.x - BORDER + 1), (int)(m_vLastWindowPos.y - maxExtents.topLeft.y - BORDER + 1),
                            (int)(m_vLastWindowSize.x + maxExtents.topLeft.x + maxExtents.bottomRight.x + 2 * BORDER - 2),
                            (int)(m_vLastWindowSize.y + maxExtents.topLeft.y + maxExtents.bottomRight.y + 2 * BORDER - 2)};
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
    static auto* const PROUNDING           = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;
    static auto* const PSHADOWIGNOREWINDOW = &g_pConfigManager->getConfigValuePtr("decoration:shadow_ignore_window")->intValue;
    static auto* const PSHADOWSCALE        = &g_pConfigManager->getConfigValuePtr("decoration:shadow_scale")->floatValue;
    static auto* const PSHADOWOFFSET       = &g_pConfigManager->getConfigValuePtr("decoration:shadow_offset")->vecValue;

    if (*PSHADOWS != 1)
        return; // disabled

    const auto ROUNDING = m_pWindow->rounding() + m_pWindow->getRealBorderSize();

    // draw the shadow
    wlr_box fullBox = {m_bLastWindowBox.x - *PSHADOWSIZE, m_bLastWindowBox.y - *PSHADOWSIZE, m_bLastWindowBox.width + 2.0 * *PSHADOWSIZE,
                       m_bLastWindowBox.height + 2.0 * *PSHADOWSIZE};

    fullBox.x -= pMonitor->vecPosition.x;
    fullBox.y -= pMonitor->vecPosition.y;

    const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.f, 1.f);

    // scale the box in relation to the center of the box
    const Vector2D NEWSIZE = Vector2D{fullBox.width, fullBox.height} * SHADOWSCALE;
    fullBox.width          = NEWSIZE.x;
    fullBox.height         = NEWSIZE.y;

    if (PSHADOWOFFSET->x < 0) {
        fullBox.x += PSHADOWOFFSET->x;
    } else if (PSHADOWOFFSET->x > 0) {
        fullBox.x = m_bLastWindowBox.x + m_bLastWindowBox.width - fullBox.width + (SHADOWSCALE * *PSHADOWSIZE) + PSHADOWOFFSET->x - pMonitor->vecPosition.x;
    } else {
        fullBox.x += ((m_bLastWindowBox.width + 2.0 * *PSHADOWSIZE) - NEWSIZE.x) / 2.0;
    }

    if (PSHADOWOFFSET->y < 0) {
        fullBox.y += PSHADOWOFFSET->y;
    } else if (PSHADOWOFFSET->y > 0) {
        fullBox.y = m_bLastWindowBox.y + m_bLastWindowBox.height - fullBox.height + (SHADOWSCALE * *PSHADOWSIZE) + PSHADOWOFFSET->y - pMonitor->vecPosition.y;
    } else {
        fullBox.y += ((m_bLastWindowBox.height + 2.0 * *PSHADOWSIZE) - NEWSIZE.y) / 2.0;
    }

    m_seExtents = {{m_bLastWindowBox.x - fullBox.x - pMonitor->vecPosition.x + 2, m_bLastWindowBox.y - fullBox.y - pMonitor->vecPosition.y + 2},
                   {fullBox.x + fullBox.width + pMonitor->vecPosition.x - m_bLastWindowBox.x - m_bLastWindowBox.width + 2,
                    fullBox.y + fullBox.height + pMonitor->vecPosition.y - m_bLastWindowBox.y - m_bLastWindowBox.height + 2}};

    fullBox.x += offset.x;
    fullBox.y += offset.y;

    if (fullBox.width < 1 || fullBox.height < 1)
        return; // don't draw invisible shadows

    g_pHyprOpenGL->scissor((wlr_box*)nullptr);

    if (*PSHADOWIGNOREWINDOW) {
        glEnable(GL_STENCIL_TEST);

        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        glStencilFunc(GL_ALWAYS, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        wlr_box windowBox = {m_bLastWindowBox.x - pMonitor->vecPosition.x, m_bLastWindowBox.y - pMonitor->vecPosition.y, m_bLastWindowBox.width, m_bLastWindowBox.height};

        scaleBox(&windowBox, pMonitor->scale);

        if (windowBox.width < 1 || windowBox.height < 1) {
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);
            glDisable(GL_STENCIL_TEST);
            return; // prevent assert failed
        }

        g_pHyprOpenGL->renderRect(&windowBox, CColor(0, 0, 0, 0), ROUNDING * pMonitor->scale);

        glStencilFunc(GL_NOTEQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    scaleBox(&fullBox, pMonitor->scale);
    g_pHyprOpenGL->renderRoundedShadow(&fullBox, ROUNDING * pMonitor->scale, *PSHADOWSIZE * pMonitor->scale, a);

    if (*PSHADOWIGNOREWINDOW) {
        // cleanup
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glDisable(GL_STENCIL_TEST);
    }
}

eDecorationLayer CHyprDropShadowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_BOTTOM;
}