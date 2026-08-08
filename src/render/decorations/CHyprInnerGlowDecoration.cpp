#include "CHyprInnerGlowDecoration.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"

#include <algorithm>

#include "../../config/ConfigManager.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../Compositor.hpp"
#include "../../desktop/state/FocusState.hpp"
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

void CHyprInnerGlowDecoration::initializeAnimations() {
    m_gradient.initializeAnimations(m_window.lock(), self(), "fadeGlow", "glowangle");
}

void CHyprInnerGlowDecoration::updateState() {
    static auto PGLOWCOL         = CConfigValue<Config::IComplexConfigValue>("decoration:glow:color");
    static auto PGLOWCOLINACTIVE = CConfigValue<Config::IComplexConfigValue>("decoration:glow:color_inactive");

    const auto  PWINDOW = m_window.lock();
    if (!PWINDOW)
        return;

    const auto TRAITS = PWINDOW->backend().traits();
    if (TRAITS.overrideRedirect || TRAITS.suggestsNoBorder) {
        m_gradient.setTarget(Config::CGradientValueData{CHyprColor(0, 0, 0, 0)}, false);
        return;
    }

    auto* const GLOWCOL         = sc<Config::CGradientValueData*>(PGLOWCOL.ptr());
    auto* const GLOWCOLINACTIVE = sc<Config::CGradientValueData*>(PGLOWCOLINACTIVE.ptr());
    if (PWINDOW == Desktop::focusState()->window()) {
        m_gradient.setTarget(*GLOWCOL);
        return;
    }

    const auto COLORINACTIVE = Config::mgr()->getConfigValue("decoration:glow:color_inactive");
    m_gradient.setTarget(COLORINACTIVE.setByUser ? *GLOWCOLINACTIVE : *GLOWCOL);
}

void CHyprInnerGlowDecoration::onWindowMap() {
    m_gradient.onWindowMap();
}

void CHyprInnerGlowDecoration::onWindowFocus() {
    m_gradient.onWindowFocus();
}

void CHyprInnerGlowDecoration::damageEntire() {
    const auto PWINDOW = m_window.lock();
    if (!validMapped(PWINDOW))
        return;

    CBox       windowBox = PWINDOW->getWindowMainSurfaceBox();

    const auto PWORKSPACE = PWINDOW->m_workspace;
    if (PWORKSPACE && PWORKSPACE->m_renderOffset->isBeingAnimated() && (PWINDOW->m_state & Desktop::View::WINDOW_STATE_PINNED) == Desktop::View::WINDOW_STATE_NONE)
        windowBox.translate(PWORKSPACE->m_renderOffset->value());
    windowBox.translate(PWINDOW->presentation().floatingOffset());

    g_pHyprRenderer->damageRegion(CRegion(windowBox));
}

void CHyprInnerGlowDecoration::updateWindow(PHLWINDOW pWindow) {
    const auto PWINDOW = m_window.lock();
    m_lastWindowPos    = PWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    m_lastWindowSize   = PWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
}

void CHyprInnerGlowDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    const auto SELF = dynamicPointerCast<CHyprInnerGlowDecoration>(self());
    if (!SELF)
        return;

    CInnerGlowPassElement::SInnerGlowData data;
    data.deco = SELF;
    data.a    = a;
    g_pHyprRenderer->addPassElement(makeUnique<CInnerGlowPassElement>(data));
}

void CHyprInnerGlowDecoration::render(PHLMONITOR pMonitor, float const& a) {
    static auto PGLOW = CConfigValue<Config::INTEGER>("decoration:glow:enabled");
    if (!*PGLOW || !visible())
        return;

    const auto PWINDOW = m_window.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto ROUNDING      = PWINDOW->presentation().rounding() > 0 ? PWINDOW->presentation().rounding() - 1 : PWINDOW->presentation().rounding();
    const auto ROUNDINGPOWER = PWINDOW->presentation().roundingPower();
    const auto PWORKSPACE    = PWINDOW->m_workspace;
    const auto WORKSPACEOFF =
        PWORKSPACE && (PWINDOW->m_state & Desktop::View::WINDOW_STATE_PINNED) == Desktop::View::WINDOW_STATE_NONE ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    CBox windowBox = {m_lastWindowPos.x, m_lastWindowPos.y, m_lastWindowSize.x, m_lastWindowSize.y};
    windowBox.translate(-pMonitor->m_position + WORKSPACEOFF + PWINDOW->presentation().floatingOffset());
    windowBox.scale(pMonitor->m_scale).round();

    if (windowBox.width < 1 || windowBox.height < 1)
        return;

    static auto PGLOWSIZE = CConfigValue<Config::INTEGER>("decoration:glow:range");
    const auto  GLOWSIZE  = sc<int>(*PGLOWSIZE);

    const auto  GRADIENT = m_gradient.renderState();

    g_pHyprRenderer->m_renderData.currentWindow = m_window;

    g_pHyprRenderer->blend(true);

    if (GRADIENT.transitioning)
        drawGlowInternal(windowBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, GLOWSIZE * pMonitor->m_scale, GRADIENT.previous, GRADIENT.current, GRADIENT.progress, a);
    else
        drawGlowInternal(windowBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, GLOWSIZE * pMonitor->m_scale, GRADIENT.current, a);

    g_pHyprRenderer->m_renderData.currentWindow.reset();
}

void CHyprInnerGlowDecoration::drawGlowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a) {
    if (box.w < 1 || box.h < 1)
        return;
    g_pHyprRenderer->blend(true);
    g_pHyprRenderer->m_renderData.currentWindow = m_window;
    g_pHyprRenderer->drawGlow(box, round, roundingPower, range, grad, a);
    g_pHyprRenderer->m_renderData.currentWindow.reset();
}

void CHyprInnerGlowDecoration::drawGlowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                                const Config::CGradientValueData& grad2, float lerp, float a) {
    if (box.w < 1 || box.h < 1)
        return;
    g_pHyprRenderer->blend(true);
    g_pHyprRenderer->m_renderData.currentWindow = m_window;
    g_pHyprRenderer->drawGlow(box, round, roundingPower, range, grad1, grad2, lerp, a);
    g_pHyprRenderer->m_renderData.currentWindow.reset();
}

eDecorationLayer CHyprInnerGlowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

bool CHyprInnerGlowDecoration::visible() {
    static auto PENABLED = CConfigValue<Config::INTEGER>("decoration:glow:enabled");
    return *PENABLED && m_window->m_ruleApplicator->decorate().valueOrDefault();
}
