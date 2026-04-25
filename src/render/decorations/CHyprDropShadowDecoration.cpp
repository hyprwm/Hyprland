#include "CHyprDropShadowDecoration.hpp"

#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../pass/ShadowPassElement.hpp"
#include "../Renderer.hpp"
#include "../pass/RectPassElement.hpp"
#include "../pass/TextureMatteElement.hpp"

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
    const auto pos     = PWINDOW->m_realPosition->value();
    const auto size    = PWINDOW->m_realSize->value();

    CBox       shadowBox = {pos.x - m_extents.topLeft.x, pos.y - m_extents.topLeft.y, pos.x + size.x + m_extents.bottomRight.x, pos.y + size.y + m_extents.bottomRight.y};

    const auto PWORKSPACE  = PWINDOW->m_workspace;
    const auto applyOffset = [&](CBox& b) {
        if (PWORKSPACE && PWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOW->m_pinned)
            b.translate(PWORKSPACE->m_renderOffset->value());
        b.translate(PWINDOW->m_floatingOffset);
    };

    applyOffset(shadowBox);

    CRegion shadowRegion(shadowBox);

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

bool CHyprDropShadowDecoration::canRender(PHLMONITOR pMonitor) {
    static auto PSHADOWS = CConfigValue<Hyprlang::INT>("decoration:shadow:enabled");
    if (*PSHADOWS != 1)
        return false; // disabled

    const auto PWINDOW = m_window.lock();

    if (!validMapped(PWINDOW))
        return false;

    if (PWINDOW->m_realShadowColor->value() == CHyprColor(0, 0, 0, 0))
        return false; // don't draw invisible shadows

    if (!PWINDOW->m_ruleApplicator->decorate().valueOrDefault())
        return false;

    if (PWINDOW->m_ruleApplicator->noShadow().valueOrDefault())
        return false;

    return true;
}

SShadowRenderData CHyprDropShadowDecoration::getRenderData(PHLMONITOR pMonitor, float const& a) {
    if (!canRender(pMonitor))
        return {};

    const auto  PWINDOW = m_window.lock();

    static auto PSHADOWSIZE   = CConfigValue<Hyprlang::INT>("decoration:shadow:range");
    static auto PSHADOWSCALE  = CConfigValue<Hyprlang::FLOAT>("decoration:shadow:scale");
    static auto PSHADOWOFFSET = CConfigValue<Hyprlang::VEC2>("decoration:shadow:offset");

    const auto  BORDERSIZE       = PWINDOW->getRealBorderSize();
    const auto  ROUNDINGBASE     = PWINDOW->rounding();
    const auto  ROUNDINGPOWER    = PWINDOW->roundingPower();
    const auto  CORRECTIONOFFSET = (BORDERSIZE * (M_SQRT2 - 1) * std::max(2.0 - ROUNDINGPOWER, 0.0));
    const auto  ROUNDING         = ROUNDINGBASE > 0 ? (ROUNDINGBASE + BORDERSIZE) - CORRECTIONOFFSET : 0;
    const auto  PWORKSPACE       = PWINDOW->m_workspace;
    const auto  WORKSPACEOFFSET  = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

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
    m_extents = {
        .topLeft =
            {
                m_lastWindowPos.x - fullBox.x - pMonitor->m_position.x + 2,
                m_lastWindowPos.y - fullBox.y - pMonitor->m_position.y + 2,
            },
        .bottomRight =
            {
                fullBox.x + fullBox.width + pMonitor->m_position.x - m_lastWindowPos.x - m_lastWindowSize.x + 2,
                fullBox.y + fullBox.height + pMonitor->m_position.y - m_lastWindowPos.y - m_lastWindowSize.y + 2,
            },
    };

    fullBox.translate(PWINDOW->m_floatingOffset);

    if (fullBox.width < 1 || fullBox.height < 1)
        return {}; // don't draw invisible shadows

    g_pHyprRenderer->m_renderData.currentWindow = m_window;

    fullBox.scale(pMonitor->m_scale).round();

    return {
        .valid         = true,
        .fullBox       = fullBox,
        .rounding      = ROUNDING,
        .roundingPower = ROUNDINGPOWER,
        .size          = *PSHADOWSIZE,
    };
}

void CHyprDropShadowDecoration::reposition() {
    if (m_extents != m_reportedExtents)
        g_pDecorationPositioner->repositionDeco(this);

    g_pHyprRenderer->m_renderData.currentWindow.reset();
}

// TODO remove
void CHyprDropShadowDecoration::render(PHLMONITOR pMonitor, float const& a) {
    auto data = getRenderData(pMonitor, a);
    if (!data.valid)
        return;

    const auto PWINDOW = m_window.lock();

    g_pHyprRenderer->disableScissor();

    drawShadowInternal(data.fullBox, data.rounding * pMonitor->m_scale, data.roundingPower, data.size * pMonitor->m_scale, PWINDOW->m_realShadowColor->value(), a);

    reposition();
}

eDecorationLayer CHyprDropShadowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_BOTTOM;
}

void CHyprDropShadowDecoration::drawShadowInternal(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) {
    static auto PSHADOWSHARP = CConfigValue<Hyprlang::INT>("decoration:shadow:sharp");

    if (box.w < 1 || box.h < 1)
        return;

    g_pHyprRenderer->blend(true);

    color.a *= a;

    if (*PSHADOWSHARP)
        g_pHyprRenderer->draw(CRectPassElement::SRectData{
            .box           = box,
            .color         = color,
            .round         = round,
            .roundingPower = roundingPower,
        });
    else
        g_pHyprRenderer->drawShadow(box, round, roundingPower, range, color, 1.F);
}
