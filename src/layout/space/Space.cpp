#include "Space.hpp"

#include "../target/Target.hpp"
#include "../algorithm/Algorithm.hpp"

#include "../../debug/log/Logger.hpp"
#include "../../desktop/Workspace.hpp"
#include "../../config/ConfigManager.hpp"

using namespace Layout;

SP<CSpace> CSpace::create(PHLWORKSPACE w) {
    auto space    = SP<CSpace>(new CSpace(w));
    space->m_self = space;
    return space;
}

CSpace::CSpace(PHLWORKSPACE parent) : m_parent(parent) {
    recheckWorkArea();
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
    if (!m_parent || !m_parent->m_monitor) {
        Log::logger->log(Log::ERR, "CSpace: recheckWorkArea on no parent / mon?!");
        return;
    }

    const auto  WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(m_parent.lock());

    auto        workArea = m_parent->m_monitor->logicalBoxMinusReserved();

    static auto PGAPSOUTDATA   = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    static auto PFLOATGAPSDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:float_gaps");
    auto* const PGAPSOUT       = sc<CCssGapData*>((PGAPSOUTDATA.ptr())->getData());
    auto*       PFLOATGAPS     = sc<CCssGapData*>(PFLOATGAPSDATA.ptr()->getData());
    if (PFLOATGAPS->m_bottom < 0 || PFLOATGAPS->m_left < 0 || PFLOATGAPS->m_right < 0 || PFLOATGAPS->m_top < 0)
        PFLOATGAPS = PGAPSOUT;

    auto                   gapsOut   = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);
    auto                   gapsFloat = WORKSPACERULE.gapsOut.value_or(*PFLOATGAPS);

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

void CSpace::recalculate() {
    recheckWorkArea();

    if (m_algorithm)
        m_algorithm->recalculate();
}

void CSpace::setFullscreen(SP<ITarget> t, eFullscreenMode mode) {
    t->setFullscreenMode(mode);

    if (mode == FSMODE_NONE && m_algorithm && t->floating())
        m_algorithm->recenter(t);

    recalculate();
}

std::expected<void, std::string> CSpace::layoutMsg(const std::string_view& sv) {
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

SP<ITarget> CSpace::getNextCandidate(SP<ITarget> old) {
    return !m_algorithm ? nullptr : m_algorithm->getNextCandidate(old);
}

const std::vector<WP<ITarget>>& CSpace::targets() const {
    return m_targets;
}
