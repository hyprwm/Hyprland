#include "Space.hpp"

#include "../target/Target.hpp"
#include "../target/WindowTarget.hpp"
#include "../algorithm/Algorithm.hpp"

#include "../../debug/log/Logger.hpp"
#include "../../workspace/HLWorkspace.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../event/EventBus.hpp"
#include "../../output/Monitor.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"

using namespace Layout;

SP<CSpace> CSpace::create(PHLWORKSPACE w) {
    auto space    = SP<CSpace>(new CSpace(w));
    space->m_self = space;
    return space;
}

CSpace::CSpace(PHLWORKSPACE parent) : m_parent(parent) {
    recheckWorkArea();

    // NOLINTNEXTLINE
    m_geomUpdateCallback = Event::bus()->m_events.monitor.layoutChanged.listen([this] {
        // During monitor disconnect/reconnect (e.g. sleep/wake), some workspaces
        // may have stale or null monitors. Guard against that to avoid crashing
        // when recalculating layout for workspaces mid-migration.
        if (!m_parent || !m_parent->m_monitor)
            return;

        recheckWorkArea();

        if (m_algorithm)
            m_algorithm->recalculate();
    });
}

void CSpace::add(SP<ITarget> t) {
    m_targets.emplace_back(t);

    recheckWorkArea();

    if (m_algorithm)
        m_algorithm->addTarget(t);

    m_parent->updateWindows();
}

void CSpace::move(SP<ITarget> t, std::optional<Vector2D> focalPoint) {
    m_targets.emplace_back(t);

    recheckWorkArea();

    if (m_algorithm)
        m_algorithm->moveTarget(t, focalPoint);

    m_parent->updateWindows();
}

void CSpace::remove(SP<ITarget> t) {
    std::erase_if(m_targets, [&t](const auto& e) { return !e || e == t; });

    recheckWorkArea();

    if (m_algorithm)
        m_algorithm->removeTarget(t);

    if (m_parent) // can be null if the workspace is gone
        m_parent->updateWindows();
}

void CSpace::setAlgorithmProvider(SP<CAlgorithm> algo) {
    m_algorithm = algo;
}

void CSpace::recheckWorkArea() {
    m_mergedBordersDirty = true;

    if (!m_parent || !m_parent->m_monitor) {
        LOG(Log::ERR, "CSpace: recheckWorkArea on no parent / mon?!");
        return;
    }

    const auto  WORKSPACERULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(m_parent.lock()).value_or(Config::CWorkspaceRule{});

    auto        workArea = m_parent->m_monitor->logicalBoxMinusReserved();

    static auto PGAPSOUTDATA   = CConfigValue<Config::IComplexConfigValue>("general:gaps_out");
    static auto PFLOATGAPSDATA = CConfigValue<Config::IComplexConfigValue>("general:float_gaps");
    auto* const PGAPSOUT       = sc<Config::CCssGapData*>(PGAPSOUTDATA.ptr());
    auto*       PFLOATGAPS     = sc<Config::CCssGapData*>(PFLOATGAPSDATA.ptr());
    if (PFLOATGAPS->m_bottom < 0 || PFLOATGAPS->m_left < 0 || PFLOATGAPS->m_right < 0 || PFLOATGAPS->m_top < 0)
        PFLOATGAPS = PGAPSOUT;

    auto                   gapsOut   = WORKSPACERULE.m_gapsOut.value_or(*PGAPSOUT);
    auto                   gapsFloat = WORKSPACERULE.m_floatGaps.value_or(*PFLOATGAPS);

    Desktop::CReservedArea reservedGaps{gapsOut.m_top, gapsOut.m_right, gapsOut.m_bottom, gapsOut.m_left};
    Desktop::CReservedArea reservedFloatGaps{gapsFloat.m_top, gapsFloat.m_right, gapsFloat.m_bottom, gapsFloat.m_left};

    auto                   floatWorkArea = workArea;

    reservedFloatGaps.applyip(floatWorkArea);
    reservedGaps.applyip(workArea);

    m_workArea         = workArea;
    m_floatingWorkArea = floatWorkArea;
}

const CBox& CSpace::workArea(bool floating) const {
    return floating ? m_floatingWorkArea : m_workArea;
}

Config::CCssGapData CSpace::gapsIn() const {
    static auto PMERGE = CConfigValue<Config::BOOL>("general:merge_borders");
    if (*PMERGE)
        return Config::CCssGapData{0, 0, 0, 0};

    static auto PGAPSIN = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
    const auto  RULE    = Config::workspaceRuleMgr()->getWorkspaceRuleFor(m_parent.lock());
    return RULE && RULE->m_gapsIn ? *RULE->m_gapsIn : *sc<Config::CCssGapData*>(PGAPSIN.ptr());
}

std::optional<int> CSpace::mergedBorderSize() {
    if (!m_mergedBordersDirty)
        return m_mergedBorderSize;

    m_mergedBordersDirty = false;
    m_mergedBorderSize.reset();

    static auto PMERGE = CConfigValue<Config::BOOL>("general:merge_borders");
    static auto PCLAMP = CConfigValue<Config::BOOL>("misc:size_limits_tiled");
    if (!*PMERGE || *PCLAMP)
        return std::nullopt;

    std::optional<int> width;
    for (const auto& target : m_targets) {
        if (!target || target->floating())
            continue;

        const auto WINDOW = target->window();
        if (!validMapped(WINDOW))
            continue;

        if (target->isPseudo() || WINDOW->windowTarget()->isPseudo() || Fullscreen::controller()->getFullscreenModes(WINDOW).internal != Fullscreen::FSMODE_NONE)
            return std::nullopt;

        const auto BORDER = WINDOW->presentation().borderSize();
        const auto EXTENT = WINDOW->presentation().borderExtent();
        // Additional decorations and non-filling targets need neighbor information to merge safely.
        // Keep this first implementation conservative across the entire workspace.
        if (EXTENT <= 0 || WINDOW->getFullWindowReservedArea() != SBoxExtents{{EXTENT, EXTENT}, {EXTENT, EXTENT}} || (width && *width != BORDER))
            return std::nullopt;

        width = BORDER;
    }

    m_mergedBorderSize = width;
    return m_mergedBorderSize;
}

void CSpace::invalidateMergedBorders() {
    m_mergedBordersDirty = true;

    static auto PMERGE = CConfigValue<Config::BOOL>("general:merge_borders");
    if (!*PMERGE || m_mergedBordersUpdateScheduled)
        return;

    m_mergedBordersUpdateScheduled = true;
    g_pEventLoopManager->doLater([self = m_self] {
        const auto SPACE = self.lock();
        if (!SPACE)
            return;

        SPACE->m_mergedBordersUpdateScheduled = false;
        if (!SPACE->m_parent || !SPACE->m_parent->m_monitor)
            return;

        // Width and decoration changes affect every tile, not just the window that changed.
        SPACE->recalculate(RECALCULATE_REASON_INVALIDATE_MONITOR_GEOMETRIES);
    });
}

PHLWORKSPACE CSpace::workspace() const {
    return m_parent.lock();
}

void CSpace::toggleTargetFloating(SP<ITarget> t) {
    t->setWasTiling(true);
    m_algorithm->setFloating(t, !t->floating());
    t->setWasTiling(false);

    m_parent->updateWindows();

    recalculate();
}

CBox CSpace::targetPositionLocal(SP<ITarget> t) const {
    return t->position().translate(-m_workArea.pos());
}

void CSpace::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    if (!m_algorithm)
        return;

    m_algorithm->resizeTarget(Δ, target, corner);
}

void CSpace::moveTarget(const Vector2D& Δ, SP<ITarget> target) {
    if (!m_algorithm)
        return;

    m_algorithm->moveTarget(Δ, target);
}

SP<CAlgorithm> CSpace::algorithm() const {
    return m_algorithm;
}

void CSpace::recalculate(eRecalculateReason reason) {
    recheckWorkArea();

    if (m_algorithm)
        m_algorithm->recalculate(reason);
}

Config::ErrorResult CSpace::layoutMsg(const std::string_view& sv) {
    if (m_algorithm)
        return m_algorithm->layoutMsg(sv);

    return {};
}

std::optional<Vector2D> CSpace::predictSizeForNewTiledTarget() {
    if (m_algorithm)
        return m_algorithm->predictSizeForNewTiledTarget();

    return std::nullopt;
}

void CSpace::swap(SP<ITarget> a, SP<ITarget> b) {
    m_mergedBordersDirty = true;

    for (auto& t : m_targets) {
        if (t == a)
            t = b;
        else if (t == b)
            t = a;
    }

    if (m_algorithm)
        m_algorithm->swapTargets(a, b);
}

void CSpace::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    if (m_algorithm)
        m_algorithm->moveTargetInDirection(t, dir, silent);
}

void CSpace::setTargetGeom(const CBox& box, SP<ITarget> target) {
    if (m_algorithm)
        m_algorithm->setTargetGeom(box, target);
}

SP<ITarget> CSpace::getNextCandidate(SP<ITarget> old) {
    return !m_algorithm ? nullptr : m_algorithm->getNextCandidate(old);
}

bool Layout::isHardRecalculateReason(eRecalculateReason reason) {
    return reason != RECALCULATE_REASON_WORKSPACE_CHANGE && reason != RECALCULATE_REASON_SPECIAL_WORKSPACE_TOGGLE &&
        reason != RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN && reason != RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN &&
        reason != RECALCULATE_REASON_INVALIDATE_MONITOR_GEOMETRIES && reason != RECALCULATE_REASON_RENDER_MONITOR;
}

const std::vector<WP<ITarget>>& CSpace::targets() const {
    return m_targets;
}

eRecalculateReason Layout::recalcMonitorReasonToRecalcReason(CLayoutManager::eRecalculateMonitorReason reason) {
    // If eRecalculateMonitorReason doesn't have a eRecalculateReason pair, it'll return nullopt
    switch (reason) {
        case CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_SPECIAL_WORKSPACE: return RECALCULATE_REASON_SPECIAL_WORKSPACE_TOGGLE;
        case CLayoutManager::RECALCULATE_MONITOR_REASON_WORKSPACE_CHANGE: return RECALCULATE_REASON_WORKSPACE_CHANGE;
        case CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_FULLSCREEN: return RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN;
        default: return RECALCULATE_REASON_UNKNOWN;
    }
}
