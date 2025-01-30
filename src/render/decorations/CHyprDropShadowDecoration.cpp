#include "CHyprDropShadowDecoration.hpp"

#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../pass/ShadowPassElement.hpp"
#include "../Renderer.hpp"

CHyprDropShadowDecoration::CHyprDropShadowDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    ;
}

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
    updateWindow(m_pWindow.lock());
}

uint64_t CHyprDropShadowDecoration::getDecorationFlags() {
    return DECORATION_NON_SOLID;
}

std::string CHyprDropShadowDecoration::getDisplayName() {
    return "Drop Shadow";
}

void CHyprDropShadowDecoration::damageEntire() {
    static auto PSHADOWS = CConfigValue<Hyprlang::INT>("decoration:shadow:enabled");

    if (*PSHADOWS != 1)
        return; // disabled

    const auto PWINDOW = m_pWindow.lock();

    CBox       shadowBox = {PWINDOW->m_vRealPosition->value().x - m_seExtents.topLeft.x, PWINDOW->m_vRealPosition->value().y - m_seExtents.topLeft.y,
                            PWINDOW->m_vRealSize->value().x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x,
                            PWINDOW->m_vRealSize->value().y + m_seExtents.topLeft.y + m_seExtents.bottomRight.y};

    const auto PWORKSPACE = PWINDOW->m_pWorkspace;
    if (PWORKSPACE && PWORKSPACE->m_vRenderOffset->isBeingAnimated() && !PWINDOW->m_bPinned)
        shadowBox.translate(PWORKSPACE->m_vRenderOffset->value());
    shadowBox.translate(PWINDOW->m_vFloatingOffset);

    static auto PSHADOWIGNOREWINDOW = CConfigValue<Hyprlang::INT>("decoration:shadow:ignore_window");
    const auto  ROUNDING            = PWINDOW->rounding();
    const auto  ROUNDINGSIZE        = ROUNDING - M_SQRT1_2 * ROUNDING + 1;

    CRegion     shadowRegion(shadowBox);
    if (*PSHADOWIGNOREWINDOW) {
        CBox surfaceBox = PWINDOW->getWindowMainSurfaceBox();
        if (PWORKSPACE && PWORKSPACE->m_vRenderOffset->isBeingAnimated() && !PWINDOW->m_bPinned)
            surfaceBox.translate(PWORKSPACE->m_vRenderOffset->value());
        surfaceBox.translate(PWINDOW->m_vFloatingOffset);
        surfaceBox.expand(-ROUNDINGSIZE);
        shadowRegion.subtract(CRegion(surfaceBox));
    }

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (!g_pHyprRenderer->shouldRenderWindow(PWINDOW, m)) {
            const CRegion monitorRegion({m->vecPosition, m->vecSize});
            shadowRegion.subtract(monitorRegion);
        }
    }

    g_pHyprRenderer->damageRegion(shadowRegion);
}

void CHyprDropShadowDecoration::updateWindow(PHLWINDOW pWindow) {
    const auto PWINDOW = m_pWindow.lock();

    m_vLastWindowPos  = PWINDOW->m_vRealPosition->value();
    m_vLastWindowSize = PWINDOW->m_vRealSize->value();

    m_bLastWindowBox          = {m_vLastWindowPos.x, m_vLastWindowPos.y, m_vLastWindowSize.x, m_vLastWindowSize.y};
    m_bLastWindowBoxWithDecos = g_pDecorationPositioner->getBoxWithIncludedDecos(pWindow);
}

void CHyprDropShadowDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    CShadowPassElement::SShadowData data;
    data.deco = this;
    data.a    = a;
    g_pHyprRenderer->m_sRenderPass.add(makeShared<CShadowPassElement>(data));
}

void CHyprDropShadowDecoration::render(PHLMONITOR pMonitor, float const& a) {
    const auto PWINDOW = m_pWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    if (PWINDOW->m_cRealShadowColor->value() == CHyprColor(0, 0, 0, 0))
        return; // don't draw invisible shadows

    if (!PWINDOW->m_sWindowData.decorate.valueOrDefault())
        return;

    if (PWINDOW->m_sWindowData.noShadow.valueOrDefault())
        return;

    static auto PSHADOWS            = CConfigValue<Hyprlang::INT>("decoration:shadow:enabled");
    static auto PSHADOWSIZE         = CConfigValue<Hyprlang::INT>("decoration:shadow:range");
    static auto PSHADOWIGNOREWINDOW = CConfigValue<Hyprlang::INT>("decoration:shadow:ignore_window");
    static auto PSHADOWSCALE        = CConfigValue<Hyprlang::FLOAT>("decoration:shadow:scale");
    static auto PSHADOWOFFSET       = CConfigValue<Hyprlang::VEC2>("decoration:shadow:offset");

    if (*PSHADOWS != 1)
        return; // disabled

    const auto ROUNDINGBASE    = PWINDOW->rounding();
    const auto ROUNDINGPOWER   = PWINDOW->roundingPower();
    const auto ROUNDING        = ROUNDINGBASE > 0 ? ROUNDINGBASE + PWINDOW->getRealBorderSize() : 0;
    const auto PWORKSPACE      = PWINDOW->m_pWorkspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_bPinned ? PWORKSPACE->m_vRenderOffset->value() : Vector2D();

    // draw the shadow
    CBox fullBox = m_bLastWindowBoxWithDecos;
    fullBox.translate(-pMonitor->vecPosition + WORKSPACEOFFSET);
    fullBox.x -= *PSHADOWSIZE;
    fullBox.y -= *PSHADOWSIZE;
    fullBox.w += 2 * *PSHADOWSIZE;
    fullBox.h += 2 * *PSHADOWSIZE;

    const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.f, 1.f);

    // scale the box in relation to the center of the box
    fullBox.scaleFromCenter(SHADOWSCALE).translate({(*PSHADOWOFFSET).x, (*PSHADOWOFFSET).y});

    updateWindow(PWINDOW);
    m_vLastWindowPos += WORKSPACEOFFSET;
    m_seExtents = {{m_vLastWindowPos.x - fullBox.x - pMonitor->vecPosition.x + 2, m_vLastWindowPos.y - fullBox.y - pMonitor->vecPosition.y + 2},
                   {fullBox.x + fullBox.width + pMonitor->vecPosition.x - m_vLastWindowPos.x - m_vLastWindowSize.x + 2,
                    fullBox.y + fullBox.height + pMonitor->vecPosition.y - m_vLastWindowPos.y - m_vLastWindowSize.y + 2}};

    fullBox.translate(PWINDOW->m_vFloatingOffset);

    if (fullBox.width < 1 || fullBox.height < 1)
        return; // don't draw invisible shadows

    g_pHyprOpenGL->scissor(nullptr);
    g_pHyprOpenGL->m_RenderData.currentWindow = m_pWindow;

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

        windowBox.translate(PWINDOW->m_vFloatingOffset);
        withDecos.translate(PWINDOW->m_vFloatingOffset);

        auto scaledExtentss = withDecos.extentsFrom(windowBox);
        scaledExtentss      = scaledExtentss * pMonitor->scale;
        scaledExtentss      = scaledExtentss.round();

        // add extents
        windowBox.scale(pMonitor->scale).round().addExtents(scaledExtentss);

        if (windowBox.width < 1 || windowBox.height < 1)
            return; // prevent assert failed

        CRegion saveDamage = g_pHyprOpenGL->m_RenderData.damage;

        g_pHyprOpenGL->m_RenderData.damage = fullBox;
        g_pHyprOpenGL->m_RenderData.damage.subtract(windowBox.copy().expand(-ROUNDING * pMonitor->scale)).intersect(saveDamage);
        g_pHyprOpenGL->m_RenderData.renderModif.applyToRegion(g_pHyprOpenGL->m_RenderData.damage);

        alphaFB.bind();

        // build the matte
        // 10-bit formats have dogshit alpha channels, so we have to use the matte to its fullest.
        // first, clear region of interest with black (fully transparent)
        g_pHyprOpenGL->renderRect(fullBox, CHyprColor(0, 0, 0, 1), 0);

        // render white shadow with the alpha of the shadow color (otherwise we clear with alpha later and shit it to 2 bit)
        drawShadowInternal(fullBox, ROUNDING * pMonitor->scale, ROUNDINGPOWER, *PSHADOWSIZE * pMonitor->scale, CHyprColor(1, 1, 1, PWINDOW->m_cRealShadowColor->value().a), a);

        // render black window box ("clip")
        g_pHyprOpenGL->renderRect(windowBox, CHyprColor(0, 0, 0, 1.0), (ROUNDING + 1 /* This fixes small pixel gaps. */) * pMonitor->scale, ROUNDINGPOWER);

        alphaSwapFB.bind();

        // alpha swap just has the shadow color. It will be the "texture" to render.
        g_pHyprOpenGL->renderRect(fullBox, PWINDOW->m_cRealShadowColor->value().stripA(), 0);

        LASTFB->bind();

        CBox monbox = {0, 0, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y};

        g_pHyprOpenGL->setMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTextureMatte(alphaSwapFB.getTexture(), monbox, alphaFB);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->setMonitorTransformEnabled(false);

        g_pHyprOpenGL->m_RenderData.damage = saveDamage;
    } else
        drawShadowInternal(fullBox, ROUNDING * pMonitor->scale, ROUNDINGPOWER, *PSHADOWSIZE * pMonitor->scale, PWINDOW->m_cRealShadowColor->value(), a);

    if (m_seExtents != m_seReportedExtents)
        g_pDecorationPositioner->repositionDeco(this);

    g_pHyprOpenGL->m_RenderData.currentWindow.reset();
}

eDecorationLayer CHyprDropShadowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_BOTTOM;
}

void CHyprDropShadowDecoration::drawShadowInternal(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) {
    static auto PSHADOWSHARP = CConfigValue<Hyprlang::INT>("decoration:shadow:sharp");

    if (box.w < 1 || box.h < 1)
        return;

    g_pHyprOpenGL->blend(true);

    color.a *= a;

    if (*PSHADOWSHARP)
        g_pHyprOpenGL->renderRect(box, color, round, roundingPower);
    else
        g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, color, 1.F);
}
