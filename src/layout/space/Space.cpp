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

    if (m_algorithm)
        m_algorithm->addTarget(t);

    m_parent->updateWindows();
}

void CSpace::move(SP<ITarget> t) {
    m_targets.emplace_back(t);

    if (m_algorithm)
        m_algorithm->moveTarget(t);

    m_parent->updateWindows();
}

void CSpace::remove(SP<ITarget> t) {
    std::erase(m_targets, t);

    if (m_algorithm)
        m_algorithm->removeTarget(t);

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

    const auto             WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(m_parent.lock());

    auto                   workArea = m_parent->m_monitor->logicalBoxMinusReserved();

    static auto            PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto* const            PGAPSOUT     = sc<CCssGapData*>((PGAPSOUTDATA.ptr())->getData());
    auto                   gapsOut      = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);

    Desktop::CReservedArea reservedGaps{gapsOut.m_top, gapsOut.m_right, gapsOut.m_bottom, gapsOut.m_left};

    reservedGaps.applyip(workArea);

    m_workArea = workArea;
}

const CBox& CSpace::workArea() const {
    return m_workArea;
}

PHLWORKSPACE CSpace::workspace() const {
    return m_parent.lock();
}

void CSpace::toggleTargetFloating(SP<ITarget> t) {
    m_algorithm->setFloating(t, !t->floating());
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
    if (m_algorithm)
        m_algorithm->recalculate();
}

void CSpace::setFullscreen(SP<ITarget> t, eFullscreenMode mode) {
    t->setFullscreenMode(mode);
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
    if (m_algorithm)
        m_algorithm->swapTargets(a, b);
}
