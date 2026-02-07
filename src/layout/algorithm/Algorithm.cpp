#include "Algorithm.hpp"

#include "FloatingAlgorithm.hpp"
#include "TiledAlgorithm.hpp"
#include "../target/WindowTarget.hpp"
#include "../space/Space.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../desktop/history/WindowHistoryTracker.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../render/Renderer.hpp"

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

void CAlgorithm::moveTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint, bool reposition) {
    const bool SHOULD_FLOAT = target->floating();

    if (SHOULD_FLOAT) {
        m_floatingTargets.emplace_back(target);
        if (reposition)
            m_floating->newTarget(target);
        else
            m_floating->movedTarget(target, focalPoint);
    } else {
        m_tiledTargets.emplace_back(target);
        if (reposition)
            m_tiled->newTarget(target);
        else
            m_tiled->movedTarget(target, focalPoint);
    }
}

SP<CSpace> CAlgorithm::space() const {
    return m_space.lock();
}

void CAlgorithm::setFloating(SP<ITarget> target, bool floating, bool reposition) {
    removeTarget(target);

    g_pHyprRenderer->damageWindow(target->window());

    target->setFloating(floating);

    moveTarget(target, std::nullopt, reposition);

    g_pHyprRenderer->damageWindow(target->window());
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
                PFULLWINDOW->layoutTarget()->setPositionGlobal(m_space->workArea());
        }

        return;
    }
}

void CAlgorithm::recenter(SP<ITarget> t) {
    if (t->floating())
        m_floating->recenter(t);
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

void CAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    auto swapFirst = [&a, &b](std::vector<WP<ITarget>>& targets) -> bool {
        auto ia = std::ranges::find(targets, a);
        auto ib = std::ranges::find(targets, b);

        if (ia != std::ranges::end(targets) && ib != std::ranges::end(targets)) {
            std::iter_swap(ia, ib);
            return true;
        } else if (ia != std::ranges::end(targets))
            *ia = b;
        else if (ib != std::ranges::end(targets))
            *ib = a;

        return false;
    };

    if (!swapFirst(m_tiledTargets))
        swapFirst(m_floatingTargets);

    const WP<IModeAlgorithm> algA = a->floating() ? WP<IModeAlgorithm>(m_floating) : WP<IModeAlgorithm>(m_tiled);
    const WP<IModeAlgorithm> algB = b->floating() ? WP<IModeAlgorithm>(m_floating) : WP<IModeAlgorithm>(m_tiled);

    algA->swapTargets(a, b);
    if (algA != algB)
        algB->swapTargets(b, a);
}

void CAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    if (t->floating())
        m_floating->moveTargetInDirection(t, dir, silent);
    else
        m_tiled->moveTargetInDirection(t, dir, silent);
}

void CAlgorithm::updateFloatingAlgo(UP<IFloatingAlgorithm>&& algo) {
    algo->m_parent = m_self;

    for (const auto& t : m_floatingTargets) {
        const auto TARGET = t.lock();
        if (!TARGET)
            continue;

        // Unhide windows when switching layouts to prevent them from being permanently lost
        const auto WINDOW = TARGET->window();
        if (WINDOW)
            WINDOW->setHidden(false);

        m_floating->removeTarget(TARGET);
        algo->newTarget(TARGET);
    }

    m_floating = std::move(algo);
}

void CAlgorithm::updateTiledAlgo(UP<ITiledAlgorithm>&& algo) {
    algo->m_parent = m_self;

    for (const auto& t : m_tiledTargets) {
        const auto TARGET = t.lock();
        if (!TARGET)
            continue;

        // Unhide windows when switching layouts to prevent them from being permanently lost
        // This is a safeguard for layouts (including third-party plugins) that use setHidden
        const auto WINDOW = TARGET->window();
        if (WINDOW)
            WINDOW->setHidden(false);

        m_tiled->removeTarget(TARGET);
        algo->newTarget(TARGET);
    }

    m_tiled = std::move(algo);
}

const UP<ITiledAlgorithm>& CAlgorithm::tiledAlgo() const {
    return m_tiled;
}

const UP<IFloatingAlgorithm>& CAlgorithm::floatingAlgo() const {
    return m_floating;
}

SP<ITarget> CAlgorithm::getNextCandidate(SP<ITarget> old) {
    if (old->floating()) {
        // use window history to determine best target
        for (const auto& w : Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {
            if (!w->m_workspace || w->m_workspace->m_space != m_space || !w->layoutTarget() || !w->layoutTarget()->space())
                continue;

            return w->layoutTarget();
        }

        // no history, fall back
    } else {
        // ask the layout
        const auto CANDIDATE = m_tiled->getNextCandidate(old);
        if (CANDIDATE)
            return CANDIDATE;

        // no candidate, fall back
    }

    // fallback: try to focus anything
    if (!m_tiledTargets.empty())
        return m_tiledTargets.back().lock();
    if (!m_floatingTargets.empty())
        return m_floatingTargets.back().lock();

    // god damn it, maybe empty?
    return nullptr;
}
