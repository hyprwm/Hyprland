#include "CHyprInnerGlowDecoration.hpp"

#include "../../Compositor.hpp"
#include "../pass/InnerGlowPassElement.hpp"
#include "../Renderer.hpp"
#include "../OpenGL.hpp"

CHyprInnerGlowDecoration::CHyprInnerGlowDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_window(pWindow) {
    ;
}

eDecorationType CHyprInnerGlowDecoration::getDecorationType() {
    return DECORATION_INNER_GLOW;
}

SDecorationPositioningInfo CHyprInnerGlowDecoration::getPositioningInfo() {
    SDecorationPositioningInfo info;
    info.policy = DECORATION_POSITION_ABSOLUTE;
    info.edges  = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;
    return info;
}

void CHyprInnerGlowDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    updateWindow(m_window.lock());
}

uint64_t CHyprInnerGlowDecoration::getDecorationFlags() {
    return DECORATION_NON_SOLID;
}

std::string CHyprInnerGlowDecoration::getDisplayName() {
    return "Inner Glow";
}

void CHyprInnerGlowDecoration::damageEntire() {
    const auto PWINDOW = m_window.lock();
    if (!validMapped(PWINDOW))
        return;

    CBox       windowBox = PWINDOW->getWindowMainSurfaceBox();

    const auto PWORKSPACE = PWINDOW->m_workspace;
    if (PWORKSPACE && PWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOW->m_pinned)
        windowBox.translate(PWORKSPACE->m_renderOffset->value());
    windowBox.translate(PWINDOW->m_floatingOffset);

    g_pHyprRenderer->damageRegion(CRegion(windowBox));
}

void CHyprInnerGlowDecoration::updateWindow(PHLWINDOW pWindow) {
    const auto PWINDOW = m_window.lock();
    m_lastWindowPos    = PWINDOW->m_realPosition->value();
    m_lastWindowSize   = PWINDOW->m_realSize->value();
}

void CHyprInnerGlowDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    CInnerGlowPassElement::SInnerGlowData data;
    data.deco = this;
    data.a    = a;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CInnerGlowPassElement>(data));
}

void CHyprInnerGlowDecoration::render(PHLMONITOR pMonitor, float const& a) {

    static auto PGLOW      = CConfigValue<Hyprlang::INT>("decoration:glow:enabled");
    static auto PGLOWRANGE = CConfigValue<Hyprlang::INT>("decoration:glow:range");
    static auto PGLOWPOWER = CConfigValue<Hyprlang::INT>("decoration:glow:render_power");

    if (!*PGLOW)
        return;

    const auto PWINDOW = m_window.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto ROUNDING      = PWINDOW->rounding() > 0 ? PWINDOW->rounding() - 1 : PWINDOW->rounding();
    const auto ROUNDINGPOWER = PWINDOW->roundingPower();
    const auto PWORKSPACE    = PWINDOW->m_workspace;
    const auto WORKSPACEOFF  = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    CBox       windowBox = {m_lastWindowPos.x, m_lastWindowPos.y, m_lastWindowSize.x, m_lastWindowSize.y};
    windowBox.translate(-pMonitor->m_position + WORKSPACEOFF + PWINDOW->m_floatingOffset);
    windowBox.scale(pMonitor->m_scale).round();

    if (windowBox.width < 1 || windowBox.height < 1)
        return;

    const int   GLOWSIZE  = *PGLOWRANGE;
    const float GLOWPOWER = *PGLOWPOWER;
    const auto  GLOWCOLOR = m_window->m_realGlowColor->value();

    g_pHyprRenderer->m_renderData.currentWindow = m_window;

    g_pHyprRenderer->blend(true);

    g_pHyprOpenGL->renderInnerGlow(windowBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, GLOWSIZE * pMonitor->m_scale, GLOWCOLOR, GLOWPOWER, a);

    g_pHyprRenderer->m_renderData.currentWindow.reset();
}

eDecorationLayer CHyprInnerGlowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}
