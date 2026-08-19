#include "CHyprBorderDecoration.hpp"
#include "../../desktop/view/window/WindowGroupMembership.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/state/WindowState.hpp"
#include "../../desktop/view/Group.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../output/WorkspaceTransition.hpp"
#include "../pass/BorderPassElement.hpp"
#include "../Renderer.hpp"
#include "../../state/MonitorState.hpp"

CHyprBorderDecoration::CHyprBorderDecoration(PHLWINDOW pWindow) :
    IHyprWindowDecoration(pWindow), m_window(pWindow), m_gradient(Config::CGradientValueData{CHyprColor(sc<uint64_t>(0))}) {
    ;
}

SDecorationPositioningInfo CHyprBorderDecoration::getPositioningInfo() {
    const auto BORDERSIZE = borderSize();
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
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP, m_window));

    const auto PWORKSPACE = m_window->m_workspace;

    if (!PWORKSPACE)
        return box;

    const auto WORKSPACEOFFSET =
        !(m_window->m_state & Desktop::View::WINDOW_STATE_PINNED) && PWORKSPACE->m_monitor ? PWORKSPACE->m_monitor->m_workspaceTransition->offsetValue(PWORKSPACE) : Vector2D{};
    return box.translate(WORKSPACEOFFSET);
}

CBox CHyprBorderDecoration::assignedBoxGlobalForRender(const Render::CRenderingContext& context) {
    CBox box = m_assignedGeometry;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP, m_window));

    if (!m_window->m_workspace || m_window->m_state & Desktop::View::WINDOW_STATE_PINNED)
        return box;

    return box.translate(g_pHyprRenderer->workspaceRenderOffset(context, m_window->m_workspace));
}

void CHyprBorderDecoration::draw(Render::CRenderingContext& context, PHLMONITOR pMonitor, float const& a) {
    if (doesntWantBorders())
        return;

    if (m_assignedGeometry.width < m_extents.topLeft.x + 1 || m_assignedGeometry.height < m_extents.topLeft.y + 1)
        return;

    CBox windowBox = assignedBoxGlobalForRender(context)
                         .translate(-pMonitor->m_position + g_pHyprRenderer->windowRenderFloatingOffset(context, m_window.lock()))
                         .expand(-borderSize())
                         .scale(pMonitor->m_scale)
                         .round();

    if (windowBox.width < 1 || windowBox.height < 1)
        return;

    const auto                      GRADIENT         = m_gradient.renderState();
    int                             borderSize       = this->borderSize();
    const auto                      ROUNDINGBASE     = m_window->presentation().rounding();
    const auto                      ROUNDING         = ROUNDINGBASE * pMonitor->m_scale;
    const auto                      ROUNDINGPOWER    = m_window->presentation().roundingPower();
    const auto                      CORRECTIONOFFSET = (borderSize * (M_SQRT2 - 1) * std::max(2.0 - ROUNDINGPOWER, 0.0));
    const auto                      OUTERROUND       = ((ROUNDINGBASE + borderSize) - CORRECTIONOFFSET) * pMonitor->m_scale;

    CBorderPassElement::SBorderData data;
    data.box           = windowBox;
    data.grad1         = GRADIENT.current;
    data.round         = ROUNDING;
    data.outerRound    = OUTERROUND;
    data.roundingPower = ROUNDINGPOWER;
    data.a             = a;
    data.borderSize    = borderSize;
    data.window        = m_window;

    if (GRADIENT.transitioning) {
        data.hasGrad2 = true;
        data.grad1    = GRADIENT.previous;
        data.grad2    = GRADIENT.current;
        data.lerp     = GRADIENT.progress;
    }

    g_pHyprRenderer->addPassElement(context, makeUnique<CBorderPassElement>(data));
}

eDecorationType CHyprBorderDecoration::getDecorationType() {
    return DECORATION_BORDER;
}

void CHyprBorderDecoration::updateWindow(PHLWINDOW) {
    auto borderSize = this->borderSize();

    if (borderSize == m_lastBorderSize)
        return;

    if (borderSize <= 0 && m_lastBorderSize <= 0)
        return;

    m_lastBorderSize = borderSize;

    g_pDecorationPositioner->repositionDeco(this);
}

void CHyprBorderDecoration::damageEntire() {
    if (!validMapped(m_window) || Fullscreen::controller()->getFullscreenModes(m_window.lock()).internal == Fullscreen::FSMODE_FULLSCREEN)
        return;

    const auto GLOBAL_BOX = assignedBoxGlobal();
    if (GLOBAL_BOX.w <= 0 || GLOBAL_BOX.h <= 0)
        return;

    const auto ROUNDING   = m_window->presentation().rounding();
    const auto BORDERSIZE = borderSize() + 1;

    CRegion    borderRegion(GLOBAL_BOX);
    borderRegion.subtract(GLOBAL_BOX.copy().expand(-(BORDERSIZE + ROUNDING)));
    borderRegion.expand(2); // pad

    const CBox borderExtents = borderRegion.getExtents();

    for (auto const& m : State::monitorState()->monitors()) {
        const CBox monitorBox = {m->m_position, m->m_size};
        if (borderExtents.intersection(monitorBox).empty())
            continue;

        if (!g_pHyprRenderer->shouldRenderWindow(m_window.lock(), m)) {
            const CRegion monitorRegion(monitorBox);
            borderRegion.subtract(monitorRegion);
        }
    }

    g_pHyprRenderer->damageRegion(borderRegion);
}

eDecorationLayer CHyprBorderDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

uint64_t CHyprBorderDecoration::getDecorationFlags() {
    static auto PPARTOFWINDOW = CConfigValue<Config::INTEGER>("decoration:border_part_of_window");

    return *PPARTOFWINDOW && !doesntWantBorders() ? DECORATION_PART_OF_MAIN_WINDOW : 0;
}

std::string CHyprBorderDecoration::getDisplayName() {
    return "Border";
}

void CHyprBorderDecoration::initializeAnimations() {
    m_gradient.initializeAnimations(m_window.lock(), self(), "border", "borderangle");
}

void CHyprBorderDecoration::updateState() {
    static auto PACTIVECOL              = CConfigValue<Config::IComplexConfigValue>("general:col.active_border");
    static auto PINACTIVECOL            = CConfigValue<Config::IComplexConfigValue>("general:col.inactive_border");
    static auto PNOGROUPACTIVECOL       = CConfigValue<Config::IComplexConfigValue>("general:col.nogroup_border_active");
    static auto PNOGROUPINACTIVECOL     = CConfigValue<Config::IComplexConfigValue>("general:col.nogroup_border");
    static auto PGROUPACTIVECOL         = CConfigValue<Config::IComplexConfigValue>("group:col.border_active");
    static auto PGROUPINACTIVECOL       = CConfigValue<Config::IComplexConfigValue>("group:col.border_inactive");
    static auto PGROUPACTIVELOCKEDCOL   = CConfigValue<Config::IComplexConfigValue>("group:col.border_locked_active");
    static auto PGROUPINACTIVELOCKEDCOL = CConfigValue<Config::IComplexConfigValue>("group:col.border_locked_inactive");

    const auto  PWINDOW = m_window.lock();
    if (!PWINDOW)
        return;

    invalidateBorderSize();

    auto* const ACTIVECOL              = sc<Config::CGradientValueData*>(PACTIVECOL.ptr());
    auto* const INACTIVECOL            = sc<Config::CGradientValueData*>(PINACTIVECOL.ptr());
    auto* const NOGROUPACTIVECOL       = sc<Config::CGradientValueData*>(PNOGROUPACTIVECOL.ptr());
    auto* const NOGROUPINACTIVECOL     = sc<Config::CGradientValueData*>(PNOGROUPINACTIVECOL.ptr());
    auto* const GROUPACTIVECOL         = sc<Config::CGradientValueData*>(PGROUPACTIVECOL.ptr());
    auto* const GROUPINACTIVECOL       = sc<Config::CGradientValueData*>(PGROUPINACTIVECOL.ptr());
    auto* const GROUPACTIVELOCKEDCOL   = sc<Config::CGradientValueData*>(PGROUPACTIVELOCKEDCOL.ptr());
    auto* const GROUPINACTIVELOCKEDCOL = sc<Config::CGradientValueData*>(PGROUPINACTIVELOCKEDCOL.ptr());

    const bool GROUPLOCKED = PWINDOW->grouping().group() ? PWINDOW->grouping().group()->locked() || Desktop::windowState()->groupsLocked() : Desktop::windowState()->groupsLocked();
    if (PWINDOW == Desktop::focusState()->window()) {
        const auto* const ACTIVECOLOR = !PWINDOW->grouping().group() ? (!(PWINDOW->grouping().rules() & Desktop::View::GROUP_DENY) ? ACTIVECOL : NOGROUPACTIVECOL) :
                                                                       (GROUPLOCKED ? GROUPACTIVELOCKEDCOL : GROUPACTIVECOL);
        m_gradient.setTarget(PWINDOW->m_ruleApplicator->activeBorderColor().valueOr(*ACTIVECOLOR));
        return;
    }

    const auto* const INACTIVECOLOR = !PWINDOW->grouping().group() ? (!(PWINDOW->grouping().rules() & Desktop::View::GROUP_DENY) ? INACTIVECOL : NOGROUPINACTIVECOL) :
                                                                     (GROUPLOCKED ? GROUPINACTIVELOCKEDCOL : GROUPINACTIVECOL);
    m_gradient.setTarget(PWINDOW->m_ruleApplicator->inactiveBorderColor().valueOr(*INACTIVECOLOR));
}

void CHyprBorderDecoration::onWindowMap() {
    m_gradient.onWindowMap();
}

void CHyprBorderDecoration::onWindowFocus() {
    m_gradient.onWindowFocus();
}

int CHyprBorderDecoration::borderSize() const {
    if (!m_borderSizeCacheDirty)
        return m_cachedBorderSize;

    const auto PWINDOW = m_window.lock();
    if (!PWINDOW)
        return 0;

    if ((PWINDOW->m_workspace && Fullscreen::controller()->getFullscreenModes(PWINDOW).internal == Fullscreen::FSMODE_FULLSCREEN) ||
        !PWINDOW->m_ruleApplicator->decorate().valueOrDefault()) {
        m_cachedBorderSize     = 0;
        m_borderSizeCacheDirty = false;
        return 0;
    }

    static auto PBORDERSIZE = CConfigValue<Config::INTEGER>("general:border_size");

    m_cachedBorderSize     = PWINDOW->m_ruleApplicator->borderSize().valueOr(*PBORDERSIZE);
    m_borderSizeCacheDirty = false;
    return m_cachedBorderSize;
}

void CHyprBorderDecoration::invalidateBorderSize() {
    m_borderSizeCacheDirty = true;
}

bool CHyprBorderDecoration::doesntWantBorders() {
    return m_window->backend().traits().suggestsNoBorder || borderSize() == 0 || !m_window->m_ruleApplicator->decorate().valueOrDefault();
}
