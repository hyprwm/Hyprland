#include "CHyprBorderDecoration.hpp"
#include "../../Compositor.hpp"

CHyprBorderDecoration::CHyprBorderDecoration(CWindow* pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;
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
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP, m_pWindow));

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(m_pWindow->m_iWorkspaceID);

    if (!PWORKSPACE)
        return box;

    const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();
    return box.translate(WORKSPACEOFFSET);
}

void CHyprBorderDecoration::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    if (doesntWantBorders())
        return;

    if (m_bAssignedGeometry.width < m_seExtents.topLeft.x + 1 || m_bAssignedGeometry.height < m_seExtents.topLeft.y + 1)
        return;

    CBox windowBox = assignedBoxGlobal().translate(-pMonitor->vecPosition + offset).expand(-m_pWindow->getRealBorderSize()).scale(pMonitor->scale).round();

    if (windowBox.width < 1 || windowBox.height < 1)
        return;

    auto       grad     = m_pWindow->m_cRealBorderColor;
    const bool ANIMATED = m_pWindow->m_fBorderFadeAnimationProgress.isBeingAnimated();
    float      a1       = a * (ANIMATED ? m_pWindow->m_fBorderFadeAnimationProgress.fl() : 1.f);

    if (m_pWindow->m_fBorderAngleAnimationProgress.getConfig()->pValues->internalEnabled) {
        grad.m_fAngle += m_pWindow->m_fBorderAngleAnimationProgress.fl() * M_PI * 2;
        grad.m_fAngle = normalizeAngleRad(grad.m_fAngle);
    }

    int        borderSize = m_pWindow->getRealBorderSize();
    const auto RADII      = m_pWindow->getCornerRadii() * pMonitor->scale;

    g_pHyprOpenGL->renderBorder(&windowBox, grad, RADII, borderSize, a1);

    if (ANIMATED) {
        float a2 = a * (1.f - m_pWindow->m_fBorderFadeAnimationProgress.fl());
        g_pHyprOpenGL->renderBorder(&windowBox, m_pWindow->m_cRealBorderColorPrevious, RADII, borderSize, a2);
    }
}

eDecorationType CHyprBorderDecoration::getDecorationType() {
    return DECORATION_BORDER;
}

void CHyprBorderDecoration::updateWindow(CWindow*) {
    if (m_pWindow->getRealBorderSize() != m_seExtents.topLeft.x)
        g_pDecorationPositioner->repositionDeco(this);
}

void CHyprBorderDecoration::damageEntire() {
    ; // ignored, done in animationManager. todo, move.
}

eDecorationLayer CHyprBorderDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

uint64_t CHyprBorderDecoration::getDecorationFlags() {
    static auto* const PPARTOFWINDOW = &g_pConfigManager->getConfigValuePtr("general:border_part_of_window")->intValue;

    return *PPARTOFWINDOW && !doesntWantBorders() ? DECORATION_PART_OF_MAIN_WINDOW : 0;
}

std::string CHyprBorderDecoration::getDisplayName() {
    return "Border";
}

bool CHyprBorderDecoration::doesntWantBorders() {
    return !m_pWindow->m_sSpecialRenderData.border || m_pWindow->m_bX11DoesntWantBorders || m_pWindow->getRealBorderSize() == 0;
}
