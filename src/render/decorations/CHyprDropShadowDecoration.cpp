#include "CHyprDropShadowDecoration.hpp"

#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../pass/ShadowPassElement.hpp"
#include "../Renderer.hpp"

CHyprDropShadowDecoration::CHyprDropShadowDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_window(pWindow) {
    ;
}

eDecorationType CHyprDropShadowDecoration::getDecorationType() {
    return DECORATION_SHADOW;
}

SDecorationPositioningInfo CHyprDropShadowDecoration::getPositioningInfo() {
    SDecorationPositioningInfo info;
    info.policy         = DECORATION_POSITION_ABSOLUTE;
    info.desiredExtents = m_extents;
    info.edges          = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;

    m_reportedExtents = m_extents;
    return info;
}

void CHyprDropShadowDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    updateWindow(m_window.lock());
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

    const auto PWINDOW = m_window.lock();

    CBox       shadowBox = {PWINDOW->m_realPosition->value().x - m_extents.topLeft.x, PWINDOW->m_realPosition->value().y - m_extents.topLeft.y,
                            PWINDOW->m_realSize->value().x + m_extents.topLeft.x + m_extents.bottomRight.x,
                            PWINDOW->m_realSize->value().y + m_extents.topLeft.y + m_extents.bottomRight.y};

    const auto PWORKSPACE = PWINDOW->m_workspace;
    if (PWORKSPACE && PWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOW->m_pinned)
        shadowBox.translate(PWORKSPACE->m_renderOffset->value());
    shadowBox.translate(PWINDOW->m_floatingOffset);

    static auto PSHADOWIGNOREWINDOW = CConfigValue<Hyprlang::INT>("decoration:shadow:ignore_window");
    const auto  ROUNDING            = PWINDOW->rounding();
    const auto  ROUNDINGSIZE        = ROUNDING - M_SQRT1_2 * ROUNDING + 1;

    CRegion     shadowRegion(shadowBox);
    if (*PSHADOWIGNOREWINDOW) {
        CBox surfaceBox = PWINDOW->getWindowMainSurfaceBox();
        if (PWORKSPACE && PWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOW->m_pinned)
            surfaceBox.translate(PWORKSPACE->m_renderOffset->value());
        surfaceBox.translate(PWINDOW->m_floatingOffset);
        surfaceBox.expand(-ROUNDINGSIZE);
        shadowRegion.subtract(CRegion(surfaceBox));
    }

    for (auto const& m : g_pCompositor->m_monitors) {
        if (!g_pHyprRenderer->shouldRenderWindow(PWINDOW, m)) {
            const CRegion monitorRegion({m->m_position, m->m_size});
            shadowRegion.subtract(monitorRegion);
        }
    }

    g_pHyprRenderer->damageRegion(shadowRegion);
}

void CHyprDropShadowDecoration::updateWindow(PHLWINDOW pWindow) {
    const auto PWINDOW = m_window.lock();

    m_lastWindowPos  = PWINDOW->m_realPosition->value();
    m_lastWindowSize = PWINDOW->m_realSize->value();

    m_lastWindowBox          = {m_lastWindowPos.x, m_lastWindowPos.y, m_lastWindowSize.x, m_lastWindowSize.y};
    m_lastWindowBoxWithDecos = g_pDecorationPositioner->getBoxWithIncludedDecos(pWindow);
}

void CHyprDropShadowDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    CShadowPassElement::SShadowData data;
    data.deco = this;
    data.a    = a;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CShadowPassElement>(data));
}

void CHyprDropShadowDecoration::render(PHLMONITOR pMonitor, float const& a) {
    const auto PWINDOW = m_window.lock();

    if (!validMapped(PWINDOW))
        return;

    if (PWINDOW->m_realShadowColor->value() == CHyprColor(0, 0, 0, 0))
        return; // don't draw invisible shadows

    if (!PWINDOW->m_windowData.decorate.valueOrDefault())
        return;

    if (PWINDOW->m_windowData.noShadow.valueOrDefault())
        return;

    static auto PSHADOWS            = CConfigValue<Hyprlang::INT>("decoration:shadow:enabled");
    static auto PSHADOWSIZE         = CConfigValue<Hyprlang::INT>("decoration:shadow:range");
    static auto PSHADOWIGNOREWINDOW = CConfigValue<Hyprlang::INT>("decoration:shadow:ignore_window");
    static auto PSHADOWSCALE        = CConfigValue<Hyprlang::FLOAT>("decoration:shadow:scale");
    static auto PSHADOWOFFSET       = CConfigValue<Hyprlang::VEC2>("decoration:shadow:offset");

    if (*PSHADOWS != 1)
        return; // disabled

    const auto BORDERSIZE       = PWINDOW->getRealBorderSize();
    const auto ROUNDINGBASE     = PWINDOW->rounding();
    const auto ROUNDINGPOWER    = PWINDOW->roundingPower();
    const auto CORRECTIONOFFSET = (BORDERSIZE * (M_SQRT2 - 1) * std::max(2.0 - ROUNDINGPOWER, 0.0));
    const auto ROUNDING         = ROUNDINGBASE > 0 ? (ROUNDINGBASE + BORDERSIZE) - CORRECTIONOFFSET : 0;
    const auto PWORKSPACE       = PWINDOW->m_workspace;
    const auto WORKSPACEOFFSET  = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    // draw the shadow
    CBox fullBox = m_lastWindowBoxWithDecos;
    fullBox.translate(-pMonitor->m_position + WORKSPACEOFFSET);
    fullBox.x -= *PSHADOWSIZE;
    fullBox.y -= *PSHADOWSIZE;
    fullBox.w += 2 * *PSHADOWSIZE;
    fullBox.h += 2 * *PSHADOWSIZE;

    const float SHADOWSCALE = std::clamp(*PSHADOWSCALE, 0.f, 1.f);

    // scale the box in relation to the center of the box
    fullBox.scaleFromCenter(SHADOWSCALE).translate({(*PSHADOWOFFSET).x, (*PSHADOWOFFSET).y});

    updateWindow(PWINDOW);
    m_lastWindowPos += WORKSPACEOFFSET;
    m_extents = {{m_lastWindowPos.x - fullBox.x - pMonitor->m_position.x + 2, m_lastWindowPos.y - fullBox.y - pMonitor->m_position.y + 2},
                 {fullBox.x + fullBox.width + pMonitor->m_position.x - m_lastWindowPos.x - m_lastWindowSize.x + 2,
                  fullBox.y + fullBox.height + pMonitor->m_position.y - m_lastWindowPos.y - m_lastWindowSize.y + 2}};

    fullBox.translate(PWINDOW->m_floatingOffset);

    if (fullBox.width < 1 || fullBox.height < 1)
        return; // don't draw invisible shadows

    g_pHyprOpenGL->scissor(nullptr);
    g_pHyprOpenGL->m_renderData.currentWindow = m_window;

    // we'll take the liberty of using this as it should not be used rn
    CFramebuffer& alphaFB     = g_pHyprOpenGL->m_renderData.pCurrentMonData->mirrorFB;
    CFramebuffer& alphaSwapFB = g_pHyprOpenGL->m_renderData.pCurrentMonData->mirrorSwapFB;
    auto*         LASTFB      = g_pHyprOpenGL->m_renderData.currentFB;

    fullBox.scale(pMonitor->m_scale).round();

    if (*PSHADOWIGNOREWINDOW) {
        CBox windowBox = m_lastWindowBox;
        CBox withDecos = m_lastWindowBoxWithDecos;

        // get window box
        windowBox.translate(-pMonitor->m_position + WORKSPACEOFFSET);
        withDecos.translate(-pMonitor->m_position + WORKSPACEOFFSET);

        windowBox.translate(PWINDOW->m_floatingOffset);
        withDecos.translate(PWINDOW->m_floatingOffset);

        auto scaledExtentss = withDecos.extentsFrom(windowBox);
        scaledExtentss      = scaledExtentss * pMonitor->m_scale;
        scaledExtentss      = scaledExtentss.round();

        // add extents
        windowBox.scale(pMonitor->m_scale).round().addExtents(scaledExtentss);

        if (windowBox.width < 1 || windowBox.height < 1)
            return; // prevent assert failed

        CRegion saveDamage = g_pHyprOpenGL->m_renderData.damage;

        g_pHyprOpenGL->m_renderData.damage = fullBox;
        g_pHyprOpenGL->m_renderData.damage.subtract(windowBox.copy().expand(-ROUNDING * pMonitor->m_scale)).intersect(saveDamage);
        g_pHyprOpenGL->m_renderData.renderModif.applyToRegion(g_pHyprOpenGL->m_renderData.damage);

        alphaFB.bind();

        // build the matte
        // 10-bit formats have dogshit alpha channels, so we have to use the matte to its fullest.
        // first, clear region of interest with black (fully transparent)
        g_pHyprOpenGL->renderRect(fullBox, CHyprColor(0, 0, 0, 1), {.round = 0});

        // render white shadow with the alpha of the shadow color (otherwise we clear with alpha later and shit it to 2 bit)
        drawShadowInternal(fullBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, *PSHADOWSIZE * pMonitor->m_scale, CHyprColor(1, 1, 1, PWINDOW->m_realShadowColor->value().a), a);

        // render black window box ("clip")
        g_pHyprOpenGL->renderRect(windowBox, CHyprColor(0, 0, 0, 1.0),
                                  {.round = (ROUNDING + 1 /* This fixes small pixel gaps. */) * pMonitor->m_scale, .roundingPower = ROUNDINGPOWER});

        alphaSwapFB.bind();

        // alpha swap just has the shadow color. It will be the "texture" to render.
        g_pHyprOpenGL->renderRect(fullBox, PWINDOW->m_realShadowColor->value().stripA(), {.round = 0});

        LASTFB->bind();

        CBox monbox = {0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y};

        g_pHyprOpenGL->pushMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTextureMatte(alphaSwapFB.getTexture(), monbox, alphaFB);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->popMonitorTransformEnabled();

        g_pHyprOpenGL->m_renderData.damage = saveDamage;
    } else
        drawShadowInternal(fullBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, *PSHADOWSIZE * pMonitor->m_scale, PWINDOW->m_realShadowColor->value(), a);

    if (m_extents != m_reportedExtents)
        g_pDecorationPositioner->repositionDeco(this);

    g_pHyprOpenGL->m_renderData.currentWindow.reset();
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
        g_pHyprOpenGL->renderRect(box, color, {.round = round, .roundingPower = roundingPower});
    else
        g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, color, 1.F);
}
