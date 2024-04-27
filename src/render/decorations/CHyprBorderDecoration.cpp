#include "CHyprBorderDecoration.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"

CHyprBorderDecoration::CHyprBorderDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;
}

CHyprBorderDecoration::~CHyprBorderDecoration() {
    ;
}

SDecorationPositioningInfo CHyprBorderDecoration::getPositioningInfo() {
    const auto BORDERSIZE = m_pWindow.lock()->getRealBorderSize();
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

    const auto PWORKSPACE = m_pWindow.lock()->m_pWorkspace;

    if (!PWORKSPACE)
        return box;

    const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow.lock()->m_bPinned ? PWORKSPACE->m_vRenderOffset.value() : Vector2D();
    return box.translate(WORKSPACEOFFSET);
}

void CHyprBorderDecoration::draw(CMonitor* pMonitor, float a) {
    if (doesntWantBorders())
        return;

    if (m_bAssignedGeometry.width < m_seExtents.topLeft.x + 1 || m_bAssignedGeometry.height < m_seExtents.topLeft.y + 1)
        return;

    CBox windowBox =
        assignedBoxGlobal().translate(-pMonitor->vecPosition + m_pWindow.lock()->m_vFloatingOffset).expand(-m_pWindow.lock()->getRealBorderSize()).scale(pMonitor->scale).round();

    if (windowBox.width < 1 || windowBox.height < 1)
        return;

    auto       grad     = m_pWindow.lock()->m_cRealBorderColor;
    const bool ANIMATED = m_pWindow.lock()->m_fBorderFadeAnimationProgress.isBeingAnimated();
    float      a1       = a * (ANIMATED ? m_pWindow.lock()->m_fBorderFadeAnimationProgress.value() : 1.f);

    if (m_pWindow.lock()->m_fBorderAngleAnimationProgress.getConfig()->pValues->internalEnabled) {
        grad.m_fAngle += m_pWindow.lock()->m_fBorderAngleAnimationProgress.value() * M_PI * 2;
        grad.m_fAngle = normalizeAngleRad(grad.m_fAngle);
    }

    int        borderSize = m_pWindow.lock()->getRealBorderSize();
    const auto ROUNDING   = m_pWindow.lock()->rounding() * pMonitor->scale;

    g_pHyprOpenGL->renderBorder(&windowBox, grad, ROUNDING, borderSize, a1);

    if (ANIMATED) {
        float a2 = a * (1.f - m_pWindow.lock()->m_fBorderFadeAnimationProgress.value());
        g_pHyprOpenGL->renderBorder(&windowBox, m_pWindow.lock()->m_cRealBorderColorPrevious, ROUNDING, borderSize, a2);
    }
}

eDecorationType CHyprBorderDecoration::getDecorationType() {
    return DECORATION_BORDER;
}

void CHyprBorderDecoration::updateWindow(PHLWINDOW) {
    if (m_pWindow.lock()->getRealBorderSize() != m_seExtents.topLeft.x)
        g_pDecorationPositioner->repositionDeco(this);
}

void CHyprBorderDecoration::damageEntire() {
    if (!validMapped(m_pWindow))
        return;

    auto       surfaceBox   = m_pWindow.lock()->getWindowMainSurfaceBox();
    const auto ROUNDING     = m_pWindow.lock()->rounding();
    const auto ROUNDINGSIZE = ROUNDING - M_SQRT1_2 * ROUNDING + 2;
    const auto BORDERSIZE   = m_pWindow.lock()->getRealBorderSize() + 1;

    const auto PWINDOWWORKSPACE = m_pWindow.lock()->m_pWorkspace;
    if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_vRenderOffset.isBeingAnimated() && !m_pWindow.lock()->m_bPinned)
        surfaceBox.translate(PWINDOWWORKSPACE->m_vRenderOffset.value());
    surfaceBox.translate(m_pWindow.lock()->m_vFloatingOffset);

    CBox surfaceBoxExpandedBorder = surfaceBox;
    surfaceBoxExpandedBorder.expand(BORDERSIZE);
    CBox surfaceBoxShrunkRounding = surfaceBox;
    surfaceBoxShrunkRounding.expand(-ROUNDINGSIZE);

    CRegion borderRegion(surfaceBoxExpandedBorder);
    borderRegion.subtract(surfaceBoxShrunkRounding);

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (!g_pHyprRenderer->shouldRenderWindow(m_pWindow.lock(), m.get())) {
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
    return !m_pWindow.lock()->m_sSpecialRenderData.border || m_pWindow.lock()->m_bX11DoesntWantBorders || m_pWindow.lock()->getRealBorderSize() == 0;
}
