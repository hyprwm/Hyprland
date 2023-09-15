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
    static auto* const PSHADOWS   = &g_pConfigManager->getConfigValuePtr("decoration:drop_shadow")->intValue;
    const auto         BORDERSIZE = m_pWindow->getRealBorderSize();

    if (*PSHADOWS != 1)
        return; // disabled
    wlr_box dm = m_pWindow->getWindowInternalBox();
    dm.x -= BORDERSIZE;
    dm.x -= BORDERSIZE;
    dm.width += 2.0 * BORDERSIZE;
    dm.height += 2.0 * BORDERSIZE;
    g_pHyprRenderer->damageBox(&dm);
}

void CHyprDropShadowDecoration::updateWindow(CWindow* pWindow) {

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    if (pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET != m_vLastWindowPos || pWindow->m_vRealSize.vec() != m_vLastWindowSize) {
        m_vLastWindowPos  = pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET;
        m_vLastWindowSize = pWindow->m_vRealSize.vec();

        damageEntire();
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

    const auto               ROUNDING   = m_pWindow->getRealRounding();
    const auto               BORDERSIZE = m_pWindow->getRealBorderSize();

    wlr_box                  windowBox            = {m_vLastWindowPos.x, m_vLastWindowPos.y, m_vLastWindowSize.x, m_vLastWindowSize.y};
    SWindowDecorationExtents m_seReservedInternal = {m_pWindow->m_vReservedInternalTopLeft.goalv(), m_pWindow->m_vReservedInternalBottomRight.goalv()};
    addExtentsToBox(&windowBox, &m_seReservedInternal);

    windowBox.x -= pMonitor->vecPosition.x + BORDERSIZE;
    windowBox.y -= pMonitor->vecPosition.y + BORDERSIZE;
    windowBox.width += 2.0 * BORDERSIZE;
    windowBox.height += 2.0 * BORDERSIZE;

    wlr_box     fullBox = {windowBox.x - *PSHADOWSIZE, windowBox.y - *PSHADOWSIZE, windowBox.width + 2.0 * *PSHADOWSIZE, windowBox.height + 2.0 * *PSHADOWSIZE};

    const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.f, 1.f);

    // scale the box in relation to the center of the box
    const Vector2D NEWSIZE = Vector2D{fullBox.width, fullBox.height} * SHADOWSCALE;
    fullBox.width          = NEWSIZE.x;
    fullBox.height         = NEWSIZE.y;

    if (PSHADOWOFFSET->x < 0) {
        fullBox.x += PSHADOWOFFSET->x;
    } else if (PSHADOWOFFSET->x > 0) {
        fullBox.x = windowBox.x + windowBox.width - fullBox.width + (SHADOWSCALE * *PSHADOWSIZE) + PSHADOWOFFSET->x - pMonitor->vecPosition.x;
    } else {
        fullBox.x += ((windowBox.width + 2.0 * *PSHADOWSIZE) - NEWSIZE.x) / 2.0;
    }

    if (PSHADOWOFFSET->y < 0) {
        fullBox.y += PSHADOWOFFSET->y;
    } else if (PSHADOWOFFSET->y > 0) {
        fullBox.y = windowBox.y + windowBox.height - fullBox.height + (SHADOWSCALE * *PSHADOWSIZE) + PSHADOWOFFSET->y - pMonitor->vecPosition.y;
    } else {
        fullBox.y += ((windowBox.height + 2.0 * *PSHADOWSIZE) - NEWSIZE.y) / 2.0;
    }

    m_seExtents.topLeft        = {std::max(0, windowBox.x - fullBox.x), std::max(0, windowBox.y - fullBox.y)};
    m_seExtents.bottomRight    = {std::max(0, (fullBox.x + fullBox.width) - (windowBox.x + windowBox.width)),
                                  std::max(0, (fullBox.y + fullBox.height) - (windowBox.y + windowBox.height))};
    m_seExtents.isReservedArea = false;

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

        scaleBox(&windowBox, pMonitor->scale);

        if (windowBox.width < 1 || windowBox.height < 1) {
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);
            glDisable(GL_STENCIL_TEST);
            return; // prevent assert failed
        }

        g_pHyprOpenGL->renderRect(&windowBox, CColor(0, 0, 0, 0), ROUNDING == 0 ? 0 : (ROUNDING + BORDERSIZE) * pMonitor->scale);

        glStencilFunc(GL_NOTEQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    scaleBox(&fullBox, pMonitor->scale);
    g_pHyprOpenGL->renderRoundedShadow(&fullBox, ROUNDING == 0 ? 0 : (ROUNDING + BORDERSIZE) * pMonitor->scale, *PSHADOWSIZE * pMonitor->scale, a);

    if (*PSHADOWIGNOREWINDOW) {
        // cleanup
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glDisable(GL_STENCIL_TEST);
    }
}
