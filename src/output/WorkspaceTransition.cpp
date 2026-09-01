#include "WorkspaceTransition.hpp"

#include "Monitor.hpp"
#include "../animation/AnimationManager.hpp"
#include "../config/shared/animation/AnimationTree.hpp"
#include "../desktop/Workspace.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"

#include <algorithm>
#include <ranges>

using namespace Monitor;

CWorkspaceTransition::CWorkspaceTransition(CMonitor& owner) : m_owner(owner) {
    ;
}

SWorkspaceTransitionState& CWorkspaceTransition::ensure(PHLWORKSPACE workspace) {
    RASSERT(workspace, "Cannot create transition state for a null workspace");

    if (const auto OWNER = workspace->m_monitor.lock(); OWNER && OWNER.get() != &m_owner)
        return OWNER->m_workspaceTransition->ensure(workspace);

    if (const auto STATE = get(workspace); STATE)
        return *STATE;

    auto state       = makeUnique<SWorkspaceTransitionState>();
    state->workspace = workspace;

    const auto CONFIG = Config::animationTree()->getAnimationPropertyConfig(workspace->m_isSpecialWorkspace ? "specialWorkspaceIn" : "workspacesIn");
    Animation::mgr()->createAnimation(Vector2D{}, state->offset, CONFIG, workspace, AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(1.F, state->alpha, CONFIG, workspace, AVARDAMAGE_ENTIRE);
    installCallbacks(*state);

    m_states.emplace_back(std::move(state));
    return *m_states.back();
}

SWorkspaceTransitionState* CWorkspaceTransition::get(PHLWORKSPACE workspace) {
    const auto IT = std::ranges::find_if(m_states, [&workspace](const auto& state) { return state->workspace == workspace; });
    return IT == m_states.end() ? nullptr : IT->get();
}

const SWorkspaceTransitionState* CWorkspaceTransition::get(PHLWORKSPACE workspace) const {
    const auto IT = std::ranges::find_if(m_states, [&workspace](const auto& state) { return state->workspace == workspace; });
    return IT == m_states.end() ? nullptr : IT->get();
}

bool CWorkspaceTransition::participates(PHLWORKSPACE workspace) const {
    const auto STATE = get(workspace);
    return STATE && (STATE->offset->isBeingAnimated() || STATE->alpha->isBeingAnimated() || STATE->forceRendering);
}

bool CWorkspaceTransition::isAnimating(PHLWORKSPACE workspace) const {
    const auto STATE = get(workspace);
    return STATE && (STATE->offset->isBeingAnimated() || STATE->alpha->isBeingAnimated());
}

bool CWorkspaceTransition::forceRendering(PHLWORKSPACE workspace) const {
    const auto STATE = get(workspace);
    return STATE && STATE->forceRendering;
}

float CWorkspaceTransition::alphaValue(PHLWORKSPACE workspace) const {
    const auto STATE = get(workspace);
    return STATE ? STATE->alpha->value() : 1.F;
}

Vector2D CWorkspaceTransition::offsetValue(PHLWORKSPACE workspace) const {
    const auto STATE = get(workspace);
    return STATE ? STATE->offset->value() : Vector2D{};
}

std::string CWorkspaceTransition::style(PHLWORKSPACE workspace) const {
    if (const auto STATE = get(workspace); STATE)
        return STATE->alpha->getStyle();

    if (!workspace)
        return {};

    const auto CONFIG = Config::animationTree()->getAnimationPropertyConfig(workspace->m_isSpecialWorkspace ? "specialWorkspaceIn" : "workspacesIn");
    return CONFIG && CONFIG->pValues ? CONFIG->pValues->internalStyle : std::string{};
}

std::vector<PHLWORKSPACE> CWorkspaceTransition::participants(std::optional<bool> special) const {
    std::vector<PHLWORKSPACE> result;
    result.reserve(m_states.size());

    for (const auto& state : m_states) {
        const auto WORKSPACE = state->workspace.lock();
        if (!WORKSPACE || (!state->offset->isBeingAnimated() && !state->alpha->isBeingAnimated() && !state->forceRendering) ||
            (special.has_value() && WORKSPACE->m_isSpecialWorkspace != *special))
            continue;

        result.emplace_back(WORKSPACE);
    }

    return result;
}

void CWorkspaceTransition::setForceRendering(PHLWORKSPACE workspace, bool forceRendering) {
    if (!workspace)
        return;

    if (const auto OWNER = workspace->m_monitor.lock(); OWNER && OWNER.get() != &m_owner) {
        OWNER->m_workspaceTransition->setForceRendering(workspace, forceRendering);
        return;
    }

    if (forceRendering) {
        ensure(workspace).forceRendering = true;
        return;
    }

    const auto STATE = get(workspace);
    if (!STATE)
        return;

    STATE->forceRendering = false;
    prune(workspace);
}

void CWorkspaceTransition::transferTo(CWorkspaceTransition& destination, PHLWORKSPACE workspace) {
    if (&destination == this || !workspace)
        return;

    const auto IT = std::ranges::find_if(m_states, [&workspace](const auto& state) { return state->workspace == workspace; });
    if (IT == m_states.end())
        return;

    destination.remove(workspace);
    auto state = std::move(*IT);
    m_states.erase(IT);

    destination.installCallbacks(*state);
    destination.m_states.emplace_back(std::move(state));
}

void CWorkspaceTransition::remove(PHLWORKSPACE workspace) {
    std::erase_if(m_states, [&workspace](const auto& state) { return state->workspace == workspace; });
}

void CWorkspaceTransition::clear() {
    for (const auto& state : m_states) {
        state->offset->resetAllCallbacks();
        state->alpha->resetAllCallbacks();
    }

    m_states.clear();
}

void CWorkspaceTransition::prune(PHLWORKSPACE workspace) {
    std::erase_if(m_states, [this, &workspace](const auto& state) {
        const auto WORKSPACE = state->workspace.lock();
        if (!WORKSPACE)
            return true;
        if (workspace && WORKSPACE != workspace)
            return false;
        if (m_owner.m_activeSpecialWorkspace == WORKSPACE)
            return false;

        return !state->offset->isBeingAnimated() && !state->alpha->isBeingAnimated() && !state->forceRendering;
    });
}

void CWorkspaceTransition::installCallbacks(SWorkspaceTransitionState& state) {
    state.offset->resetAllCallbacks();
    state.alpha->resetAllCallbacks();

    state.offset->setUpdateCallback([workspace = state.workspace](auto) {
        const auto WORKSPACE = workspace.lock();
        if (!WORKSPACE)
            return;

        for (const auto& window : Desktop::windowState()->windows()) {
            if (!validMapped(window) || window->m_workspace != WORKSPACE)
                continue;

            window->presentation().onWorkspaceAnimUpdate();
        }
    });

    const auto DEFER_PRUNE = [monitor = PHLMONITORREF{m_owner.m_self}, workspace = state.workspace](auto) {
        if (!g_pEventLoopManager)
            return;

        g_pEventLoopManager->doLater([monitor, workspace] {
            if (!monitor || !monitor->m_workspaceTransition)
                return;

            monitor->m_workspaceTransition->prune(workspace.lock());
        });
    };

    state.offset->setCallbackOnEnd(DEFER_PRUNE, false);
    state.alpha->setCallbackOnEnd(DEFER_PRUNE, false);
}
