#include "Algorithm.hpp"

#include "FloatingAlgorithm.hpp"
#include "TiledAlgorithm.hpp"
#include "../target/Target.hpp"

#include "../../debug/log/Logger.hpp"

using namespace Layout;

SP<CAlgorithm> CAlgorithm::create(UP<ITiledAlgorithm>&& tiled, UP<IFloatingAlgorithm>&& floating, SP<CSpace> space) {
    auto algo                  = SP<CAlgorithm>(new CAlgorithm(std::move(tiled), std::move(floating), space));
    algo->m_self               = algo;
    algo->m_tiled->m_parent    = algo;
    algo->m_floating->m_parent = algo;
    return algo;
}

CAlgorithm::CAlgorithm(UP<ITiledAlgorithm>&& tiled, UP<IFloatingAlgorithm>&& floating, SP<CSpace> space) :
    m_tiled(std::move(tiled)), m_floating(std::move(floating)), m_space(space) {
    ;
}

void CAlgorithm::addTarget(SP<ITarget> target) {
    const bool SHOULD_FLOAT = target->shouldBeFloated();

    if (SHOULD_FLOAT) {
        m_floatingTargets.emplace_back(target);
        m_floating->newTarget(target);
    } else {
        m_tiledTargets.emplace_back(target);
        m_tiled->newTarget(target);
    }
}

void CAlgorithm::removeTarget(SP<ITarget> target) {
    const bool IS_FLOATING = std::ranges::contains(m_floatingTargets, target);

    if (IS_FLOATING) {
        m_floating->removeTarget(target);
        std::erase(m_floatingTargets, target);
        return;
    }

    const bool IS_TILED = std::ranges::contains(m_tiledTargets, target);

    if (IS_TILED) {
        m_tiled->removeTarget(target);
        std::erase(m_tiledTargets, target);
        return;
    }

    Log::logger->log(Log::ERR, "BUG THIS: CAlgorithm::removeTarget, but not found");
}

SP<CSpace> CAlgorithm::space() const {
    return m_space.lock();
}
