#include "Algorithm.hpp"

#include "FloatingAlgorithm.hpp"
#include "TiledAlgorithm.hpp"
#include "../target/WindowTarget.hpp"
#include "../space/Space.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../helpers/Monitor.hpp"

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
    const bool SHOULD_FLOAT = target->floating();

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

void CAlgorithm::moveTarget(SP<ITarget> target) {
    const bool SHOULD_FLOAT = target->floating();

    if (SHOULD_FLOAT) {
        m_floatingTargets.emplace_back(target);
        m_floating->movedTarget(target);
    } else {
        m_tiledTargets.emplace_back(target);
        m_tiled->movedTarget(target);
    }
}

SP<CSpace> CAlgorithm::space() const {
    return m_space.lock();
}

void CAlgorithm::setFloating(SP<ITarget> target, bool floating) {
    removeTarget(target);

    target->setFloating(floating);

    moveTarget(target);
}

size_t CAlgorithm::tiledTargets() const {
    return m_tiledTargets.size();
}

size_t CAlgorithm::floatingTargets() const {
    return m_floatingTargets.size();
}

void CAlgorithm::recalculate() {
    m_tiled->recalculate();
    m_floating->recalculate();

    const auto PWORKSPACE = m_space->workspace();
    const auto PMONITOR   = PWORKSPACE->m_monitor;

    if (PWORKSPACE->m_hasFullscreenWindow && PMONITOR) {
        // massive hack from the fullscreen func
        const auto PFULLWINDOW = PWORKSPACE->getFullscreenWindow();

        if (PFULLWINDOW) {
            if (PWORKSPACE->m_fullscreenMode == FSMODE_FULLSCREEN) {
                *PFULLWINDOW->m_realPosition = PMONITOR->m_position;
                *PFULLWINDOW->m_realSize     = PMONITOR->m_size;
            } else if (PWORKSPACE->m_fullscreenMode == FSMODE_MAXIMIZED)
                PFULLWINDOW->m_target->setPositionGlobal(m_space->workArea());
        }

        return;
    }
}

std::expected<void, std::string> CAlgorithm::layoutMsg(const std::string_view& sv) {
    if (const auto ret = m_floating->layoutMsg(sv); !ret)
        return ret;
    return m_tiled->layoutMsg(sv);
}

std::optional<Vector2D> CAlgorithm::predictSizeForNewTiledTarget() {
    return m_tiled->predictSizeForNewTarget();
}

void CAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    if (target->floating())
        m_floating->resizeTarget(Δ, target, corner);
    else
        m_tiled->resizeTarget(Δ, target, corner);
}

void CAlgorithm::moveTarget(const Vector2D& Δ, SP<ITarget> target) {
    if (target->floating())
        m_floating->moveTarget(Δ, target);
}
