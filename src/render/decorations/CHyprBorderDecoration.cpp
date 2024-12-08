#include "CHyprBorderDecoration.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../pass/BorderPassElement.hpp"

CHyprBorderDecoration::CHyprBorderDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    ;
}

CHyprBorderDecoration::~CHyprBorderDecoration() {
    ;
}

SDecorationPositioningInfo CHyprBorderDecoration::getPositioningInfo() {
    const auto BORDERSIZE = m_pWindow->getRealBorderSize();
    m_seExtents           = {{BORDERSIZE, BORDERSIZE}, {BORDERSIZE, BORDERSIZE}};

    if (doesntWantBorders())
        m_seExtents = {{}, {}};

    SDecorationPositioningInfo info;
    info.priority       = 10000;
    info.policy         = DECORATION_POSITION_STICKY;
    info.desiredExtents = m_seExtents;
    info.reserved       = true;
    info.edges          = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;

    m_seReportedExtents = m_seExtents;
    return info;
}

void CHyprBorderDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    m_bAssignedGeometry = reply.assignedGeometry;
}

CBox CHyprBorderDecoration::assignedBoxGlobal() {
    CBox box = m_bAssignedGeometry;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP, m_pWindow.lock()));

    const auto PWORKSPACE = m_pWindow->m_pWorkspace;

    if (!PWORKSPACE)
        return box;

    const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.value() : Vector2D();
    return box.translate(WORKSPACEOFFSET);
}

void CHyprBorderDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    if (doesntWantBorders())
        return;

    if (m_bAssignedGeometry.width < m_seExtents.topLeft.x + 1 || m_bAssignedGeometry.height < m_seExtents.topLeft.y + 1)
        return;

    CBox windowBox = assignedBoxGlobal().translate(-pMonitor->vecPosition + m_pWindow->m_vFloatingOffset).expand(-m_pWindow->getRealBorderSize()).scale(pMonitor->scale).round();

    if (windowBox.width < 1 || windowBox.height < 1)
        return;

    auto       grad     = m_pWindow->m_cRealBorderColor;
    const bool ANIMATED = m_pWindow->m_fBorderFadeAnimationProgress.isBeingAnimated();

    if (m_pWindow->m_fBorderAngleAnimationProgress.getConfig()->pValues->internalEnabled) {
        grad.m_fAngle += m_pWindow->m_fBorderAngleAnimationProgress.value() * M_PI * 2;
        grad.m_fAngle = normalizeAngleRad(grad.m_fAngle);
    }

    int        borderSize = m_pWindow->getRealBorderSize();
    const auto ROUNDING   = m_pWindow->rounding() * pMonitor->scale;

    CBorderPassElement::SBorderData data;
    data.box = windowBox;
    data.grad1 = grad;
    data.round = ROUNDING;
    data.a = a;
    data.borderSize = borderSize;

    if (ANIMATED) {
        data.hasGrad2 = true;
        data.grad1 = m_pWindow->m_cRealBorderColorPrevious;
        data.grad2 = grad;
        data.lerp = m_pWindow->m_fBorderFadeAnimationProgress.value();
    }

    g_pHyprRenderer->m_sRenderPass.add(makeShared<CBorderPassElement>(data));
}

eDecorationType CHyprBorderDecoration::getDecorationType() {
    return DECORATION_BORDER;
}

void CHyprBorderDecoration::updateWindow(PHLWINDOW) {
    auto borderSize = m_pWindow->getRealBorderSize();

    if (borderSize == m_iLastBorderSize)
        return;

    if (borderSize <= 0 && m_iLastBorderSize <= 0)
        return;

    m_iLastBorderSize = borderSize;

    g_pDecorationPositioner->repositionDeco(this);
}

void CHyprBorderDecoration::damageEntire() {
    if (!validMapped(m_pWindow))
        return;

    auto       surfaceBox   = m_pWindow->getWindowMainSurfaceBox();
    const auto ROUNDING     = m_pWindow->rounding();
    const auto ROUNDINGSIZE = ROUNDING - M_SQRT1_2 * ROUNDING + 2;
    const auto BORDERSIZE   = m_pWindow->getRealBorderSize() + 1;

    const auto PWINDOWWORKSPACE = m_pWindow->m_pWorkspace;
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_vRenderOffset.isBeingAnimated() && !m_pWindow->m_bPinned)
        surfaceBox.translate(PWINDOWWORKSPACE->m_vRenderOffset.value());
    surfaceBox.translate(m_pWindow->m_vFloatingOffset);

    CBox surfaceBoxExpandedBorder = surfaceBox;
    surfaceBoxExpandedBorder.expand(BORDERSIZE);
    CBox surfaceBoxShrunkRounding = surfaceBox;
    surfaceBoxShrunkRounding.expand(-ROUNDINGSIZE);

    CRegion borderRegion(surfaceBoxExpandedBorder);
    borderRegion.subtract(surfaceBoxShrunkRounding);

    for (auto const& m : g_pCompositor->m_vMonitors) {
        if (!g_pHyprRenderer->shouldRenderWindow(m_pWindow.lock(), m)) {
            const CRegion monitorRegion({m->vecPosition, m->vecSize});
            borderRegion.subtract(monitorRegion);
        }
    }

    g_pHyprRenderer->damageRegion(borderRegion);
}

eDecorationLayer CHyprBorderDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

uint64_t CHyprBorderDecoration::getDecorationFlags() {
    static auto PPARTOFWINDOW = CConfigValue<Hyprlang::INT>("general:border_part_of_window");

    return *PPARTOFWINDOW && !doesntWantBorders() ? DECORATION_PART_OF_MAIN_WINDOW : 0;
}

std::string CHyprBorderDecoration::getDisplayName() {
    return "Border";
}

bool CHyprBorderDecoration::doesntWantBorders() {
    return m_pWindow->m_sWindowData.noBorder.valueOrDefault() || m_pWindow->m_bX11DoesntWantBorders || m_pWindow->getRealBorderSize() == 0;
}
