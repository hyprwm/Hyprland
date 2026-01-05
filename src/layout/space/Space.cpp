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
}

void CSpace::remove(SP<ITarget> t) {
    std::erase(m_targets, t);

    if (m_algorithm)
        m_algorithm->removeTarget(t);
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
