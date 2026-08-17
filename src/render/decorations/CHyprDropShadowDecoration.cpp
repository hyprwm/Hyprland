#include "CHyprDropShadowDecoration.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"

#include <algorithm>
#include "../../Compositor.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../output/WorkspaceTransition.hpp"
#include "../pass/ShadowPassElement.hpp"
#include "../Renderer.hpp"
#include "../pass/RectPassElement.hpp"
#include "../pass/TextureMatteElement.hpp"
#include "../../state/MonitorState.hpp"

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

void CHyprDropShadowDecoration::initializeAnimations() {
    m_gradient.initializeAnimations(m_window.lock(), self(), "fadeShadow", "shadowangle");
}

void CHyprDropShadowDecoration::updateState() {
    static auto PSHADOWCOL         = CConfigValue<Config::IComplexConfigValue>("decoration:shadow:color");
    static auto PSHADOWCOLINACTIVE = CConfigValue<Config::IComplexConfigValue>("decoration:shadow:color_inactive");

    const auto  PWINDOW = m_window.lock();
    if (!PWINDOW)
        return;

    const auto TRAITS = PWINDOW->backend().traits();
    if (TRAITS.overrideRedirect || TRAITS.suggestsNoBorder) {
        m_gradient.setTarget(Config::CGradientValueData{CHyprColor(0, 0, 0, 0)}, false);
        return;
    }

    auto* const SHADOWCOL         = sc<Config::CGradientValueData*>(PSHADOWCOL.ptr());
    auto* const SHADOWCOLINACTIVE = sc<Config::CGradientValueData*>(PSHADOWCOLINACTIVE.ptr());
    if (PWINDOW == Desktop::focusState()->window()) {
        m_gradient.setTarget(*SHADOWCOL);
        return;
    }

    const auto COLORINACTIVE = Config::mgr()->getConfigValue("decoration:shadow:color_inactive");
    m_gradient.setTarget(COLORINACTIVE.setByUser ? *SHADOWCOLINACTIVE : *SHADOWCOL);
}

void CHyprDropShadowDecoration::onWindowMap() {
    m_gradient.onWindowMap();
}

void CHyprDropShadowDecoration::onWindowFocus() {
    m_gradient.onWindowFocus();
}

void CHyprDropShadowDecoration::damageEntire() {
    static auto PSHADOWS = CConfigValue<Config::INTEGER>("decoration:shadow:enabled");

    if (*PSHADOWS != 1)
        return; // disabled

    const auto PWINDOW = m_window.lock();
    const auto pos     = PWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    const auto size    = PWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);

    CBox       shadowBox = {pos.x - m_extents.topLeft.x, pos.y - m_extents.topLeft.y, pos.x + size.x + m_extents.bottomRight.x, pos.y + size.y + m_extents.bottomRight.y};

    const auto PWORKSPACE  = PWINDOW->m_workspace;
    const auto applyOffset = [&](CBox& b) {
        if (g_pHyprRenderer->workspaceRenderIsAnimating(PWORKSPACE) && !(PWINDOW->m_state & Desktop::View::WINDOW_STATE_PINNED))
            b.translate(g_pHyprRenderer->workspaceRenderOffset(PWORKSPACE));
        b.translate(g_pHyprRenderer->windowRenderFloatingOffset(PWINDOW));
    };

    applyOffset(shadowBox);

    CRegion shadowRegion(shadowBox);

    for (auto const& m : State::monitorState()->monitors()) {
        if (!g_pHyprRenderer->shouldRenderWindow(PWINDOW, m)) {
            const CRegion monitorRegion({m->m_position, m->m_size});
            shadowRegion.subtract(monitorRegion);
        }
    }

    g_pHyprRenderer->damageRegion(shadowRegion);
}

void CHyprDropShadowDecoration::updateWindow(PHLWINDOW pWindow) {
    const auto PWINDOW = m_window.lock();

    m_lastWindowPos  = PWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    m_lastWindowSize = PWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);

    m_lastWindowBox          = {m_lastWindowPos.x, m_lastWindowPos.y, m_lastWindowSize.x, m_lastWindowSize.y};
    m_lastWindowBoxWithDecos = g_pDecorationPositioner->getBoxWithIncludedDecos(pWindow);
}

void CHyprDropShadowDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    const auto SELF = dynamicPointerCast<CHyprDropShadowDecoration>(self());
    if (!SELF)
        return;

    CShadowPassElement::SShadowData data;
    data.deco = SELF;
    data.a    = a;
    g_pHyprRenderer->addPassElement(makeUnique<CShadowPassElement>(data));
}

bool CHyprDropShadowDecoration::canRender(PHLMONITOR pMonitor) {
    static auto PSHADOWS = CConfigValue<Config::INTEGER>("decoration:shadow:enabled");
    if (*PSHADOWS != 1)
        return false; // disabled

    const auto PWINDOW = m_window.lock();

    if (!validMapped(PWINDOW))
        return false;

    {
        static constexpr auto HAS_ALPHA = [](const auto& c) { return c.a > 0.001f; };
        const auto            GRADIENT  = m_gradient.renderState();
        if (std::none_of(GRADIENT.current.m_colors.begin(), GRADIENT.current.m_colors.end(), HAS_ALPHA)) {
            if (!GRADIENT.transitioning)
                return false;
            if (std::none_of(GRADIENT.previous.m_colors.begin(), GRADIENT.previous.m_colors.end(), HAS_ALPHA))
                return false;
        }
    }

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

    static auto PSHADOWSIZE   = CConfigValue<Config::INTEGER>("decoration:shadow:range");
    static auto PSHADOWSCALE  = CConfigValue<Config::FLOAT>("decoration:shadow:scale");
    static auto PSHADOWOFFSET = CConfigValue<Config::VEC2>("decoration:shadow:offset");

    const auto  BORDERSIZE       = PWINDOW->presentation().borderSize();
    const auto  ROUNDINGBASE     = PWINDOW->presentation().rounding();
    const auto  ROUNDINGPOWER    = PWINDOW->presentation().roundingPower();
    const auto  CORRECTIONOFFSET = (BORDERSIZE * (M_SQRT2 - 1) * std::max(2.0 - ROUNDINGPOWER, 0.0));
    const auto  ROUNDING         = ROUNDINGBASE > 0 ? (ROUNDINGBASE + BORDERSIZE) - CORRECTIONOFFSET : 0;
    const auto  PWORKSPACE       = PWINDOW->m_workspace;
    const auto  WORKSPACEOFFSET  = PWORKSPACE && !(PWINDOW->m_state & Desktop::View::WINDOW_STATE_PINNED) ? g_pHyprRenderer->workspaceRenderOffset(PWORKSPACE) : Vector2D{};

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

    fullBox.translate(g_pHyprRenderer->windowRenderFloatingOffset(PWINDOW));

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

    const auto GRADIENT = m_gradient.renderState();

    if (GRADIENT.transitioning)
        drawShadowInternal(data.fullBox, data.rounding * pMonitor->m_scale, data.roundingPower, data.size * pMonitor->m_scale, GRADIENT.previous, GRADIENT.current,
                           GRADIENT.progress, a);
    else
        drawShadowInternal(data.fullBox, data.rounding * pMonitor->m_scale, data.roundingPower, data.size * pMonitor->m_scale, GRADIENT.current, a);

    reposition();
}

eDecorationLayer CHyprDropShadowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_BOTTOM;
}

void CHyprDropShadowDecoration::drawShadowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a) {
    static auto PSHADOWSHARP = CConfigValue<Config::INTEGER>("decoration:shadow:sharp");

    if (box.w < 1 || box.h < 1)
        return;

    g_pHyprRenderer->blend(true);

    if (*PSHADOWSHARP) {
        CHyprColor flatColor = grad.m_colors.empty() ? CHyprColor(0, 0, 0, 0) : grad.m_colors[0];
        flatColor.a *= a;
        g_pHyprRenderer->draw(
            CRectPassElement::SRectData{
                .box           = box,
                .color         = flatColor,
                .round         = round,
                .roundingPower = roundingPower,
            },
            box);
    } else
        g_pHyprRenderer->drawShadow(box, round, roundingPower, range, grad, a);
}

void CHyprDropShadowDecoration::drawShadowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                                   const Config::CGradientValueData& grad2, float lerp, float a) {
    static auto PSHADOWSHARP = CConfigValue<Config::INTEGER>("decoration:shadow:sharp");

    if (box.w < 1 || box.h < 1)
        return;

    g_pHyprRenderer->blend(true);

    if (*PSHADOWSHARP) {
        CHyprColor col1 = grad1.m_colors.empty() ? CHyprColor(0, 0, 0, 0) : grad1.m_colors[0];
        CHyprColor col2 = grad2.m_colors.empty() ? col1 : grad2.m_colors[0];
        CHyprColor flatColor =
            CHyprColor(col1.r + (col2.r - col1.r) * lerp, col1.g + (col2.g - col1.g) * lerp, col1.b + (col2.b - col1.b) * lerp, col1.a + (col2.a - col1.a) * lerp);
        flatColor.a *= a;
        g_pHyprRenderer->draw(
            CRectPassElement::SRectData{
                .box           = box,
                .color         = flatColor,
                .round         = round,
                .roundingPower = roundingPower,
            },
            box);
    } else
        g_pHyprRenderer->drawShadow(box, round, roundingPower, range, grad1, grad2, lerp, a);
}
