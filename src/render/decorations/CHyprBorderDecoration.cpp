#include "CHyprBorderDecoration.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../pass/BorderPassElement.hpp"
#include "../Renderer.hpp"
#include "../../managers/HookSystemManager.hpp"

CHyprBorderDecoration::CHyprBorderDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_window(pWindow) {
    ;
}

SDecorationPositioningInfo CHyprBorderDecoration::getPositioningInfo() {
    const auto BORDERSIZE = m_window->getRealBorderSize();
    m_extents             = {{BORDERSIZE, BORDERSIZE}, {BORDERSIZE, BORDERSIZE}};

    if (doesntWantBorders())
        m_extents = {{}, {}};

    SDecorationPositioningInfo info;
    info.priority       = 10000;
    info.policy         = DECORATION_POSITION_STICKY;
    info.desiredExtents = m_extents;
    info.reserved       = true;
    info.edges          = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;

    m_reportedExtents = m_extents;
    return info;
}

void CHyprBorderDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    m_assignedGeometry = reply.assignedGeometry;
}

CBox CHyprBorderDecoration::assignedBoxGlobal() {
    CBox box = m_assignedGeometry;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP, m_window.lock()));

    const auto PWORKSPACE = m_window->m_workspace;

    if (!PWORKSPACE)
        return box;

    const auto WORKSPACEOFFSET = PWORKSPACE && !m_window->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();
    return box.translate(WORKSPACEOFFSET);
}

void CHyprBorderDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    if (doesntWantBorders())
        return;

    if (m_assignedGeometry.width < m_extents.topLeft.x + 1 || m_assignedGeometry.height < m_extents.topLeft.y + 1)
        return;

    CBox windowBox = assignedBoxGlobal().translate(-pMonitor->m_position + m_window->m_floatingOffset).expand(-m_window->getRealBorderSize()).scale(pMonitor->m_scale).round();

    if (windowBox.width < 1 || windowBox.height < 1)
        return;

    auto       grad     = m_window->m_realBorderColor;
    const bool ANIMATED = m_window->m_borderFadeAnimationProgress->isBeingAnimated();

    if (m_window->m_borderAngleAnimationProgress->enabled()) {
        grad.m_angle += m_window->m_borderAngleAnimationProgress->value() * M_PI * 2;
        grad.m_angle = normalizeAngleRad(grad.m_angle);

        // When borderangle is animated, it is counterintuitive to fade between inactive/active gradient angles.
        // Instead we sync the angles to avoid fading between them and additionally rotating the border angle.
        if (ANIMATED)
            m_window->m_realBorderColorPrevious.m_angle = grad.m_angle;
    }

    int                             borderSize       = m_window->getRealBorderSize();
    const auto                      ROUNDINGBASE     = m_window->rounding();
    const auto                      ROUNDING         = ROUNDINGBASE * pMonitor->m_scale;
    const auto                      ROUNDINGPOWER    = m_window->roundingPower();
    const auto                      CORRECTIONOFFSET = (borderSize * (M_SQRT2 - 1) * std::max(2.0 - ROUNDINGPOWER, 0.0));
    const auto                      OUTERROUND       = ((ROUNDINGBASE + borderSize) - CORRECTIONOFFSET) * pMonitor->m_scale;

    CBorderPassElement::SBorderData data;
    data.box           = windowBox;
    data.grad1         = grad;
    data.round         = ROUNDING;
    data.outerRound    = OUTERROUND;
    data.roundingPower = ROUNDINGPOWER;
    data.a             = a;
    data.borderSize    = borderSize;

    if (ANIMATED) {
        data.hasGrad2 = true;
        data.grad1    = m_window->m_realBorderColorPrevious;
        data.grad2    = grad;
        data.lerp     = m_window->m_borderFadeAnimationProgress->value();
    }

    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(data));
}

eDecorationType CHyprBorderDecoration::getDecorationType() {
    return DECORATION_BORDER;
}

void CHyprBorderDecoration::updateWindow(PHLWINDOW) {
    auto borderSize = m_window->getRealBorderSize();

    if (borderSize == m_lastBorderSize)
        return;

    if (borderSize <= 0 && m_lastBorderSize <= 0)
        return;

    m_lastBorderSize = borderSize;

    g_pDecorationPositioner->repositionDeco(this);
}

void CHyprBorderDecoration::damageEntire() {
    if (!validMapped(m_window))
        return;

    auto       surfaceBox   = m_window->getWindowMainSurfaceBox();
    const auto ROUNDING     = m_window->rounding();
    const auto ROUNDINGSIZE = ROUNDING - M_SQRT1_2 * ROUNDING + 2;
    const auto BORDERSIZE   = m_window->getRealBorderSize() + 1;

    const auto PWINDOWWORKSPACE = m_window->m_workspace;
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated() && !m_window->m_pinned)
        surfaceBox.translate(PWINDOWWORKSPACE->m_renderOffset->value());
    surfaceBox.translate(m_window->m_floatingOffset);

    CBox surfaceBoxExpandedBorder = surfaceBox;
    surfaceBoxExpandedBorder.expand(BORDERSIZE);
    CBox surfaceBoxShrunkRounding = surfaceBox;
    surfaceBoxShrunkRounding.expand(-ROUNDINGSIZE);

    CRegion borderRegion(surfaceBoxExpandedBorder);
    borderRegion.subtract(surfaceBoxShrunkRounding);

    for (auto const& m : g_pCompositor->m_monitors) {
        if (!g_pHyprRenderer->shouldRenderWindow(m_window.lock(), m)) {
            const CRegion monitorRegion({m->m_position, m->m_size});
            borderRegion.subtract(monitorRegion);
        }
    }

    g_pHyprRenderer->damageRegion(borderRegion);
}

eDecorationLayer CHyprBorderDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

uint64_t CHyprBorderDecoration::getDecorationFlags() {
    static auto PPARTOFWINDOW = CConfigValue<Hyprlang::INT>("decoration:border_part_of_window");

    return *PPARTOFWINDOW && !doesntWantBorders() ? DECORATION_PART_OF_MAIN_WINDOW : 0;
}

std::string CHyprBorderDecoration::getDisplayName() {
    return "Border";
}

bool CHyprBorderDecoration::doesntWantBorders() {
    return m_window->m_windowData.noBorder.valueOrDefault() || m_window->m_X11DoesntWantBorders || m_window->getRealBorderSize() == 0 ||
        !m_window->m_windowData.decorate.valueOrDefault();
}
