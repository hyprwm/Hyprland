#include "FullscreenHandler.hpp"

#include "../../../animation/WorkspaceAnimationController.hpp"
#include "../../../managers/fullscreen/FullscreenController.hpp"

#include "../../../debug/log/Logger.hpp"

#include "../../../desktop/DesktopTypes.hpp"
#include "../../../desktop/view/LayerSurface.hpp"
#include "../../../desktop/state/LayerState.hpp"
#include "../../../desktop/state/WindowState.hpp"

#include "../../../layout/algorithm/Algorithm.hpp"
#include "../../../layout/target/Target.hpp"

#include "../../../output/Monitor.hpp"
#include "layout/target/WindowGroupTarget.hpp"
#include <hyprutils/memory/Casts.hpp>

using namespace Fullscreen;

IFullscreenHandler::IFullscreenHandler(Layout::IModeAlgorithm* const algorithm) : m_algorithm(algorithm) {
    if (!m_algorithm) {
        Log::logger->log(Log::CRIT, "IFullscreenHandler failed during construction: Owning layout algorithm does not exist!");
        throw std::runtime_error("CScrollingFullscreenHandler: bad algorithm type");
    }
};

bool IFullscreenHandler::isFullscreen(SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    // Mode checking logic is the same as getFullscreenModes() - keep it in sync

    // isFullscreen() queries FS state; negating it to check "is target not fullscreen" is the correct way to do this.
    if (mode.has_value() && mode.value() == FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen. This must never happpen.");
        return false;
    }

    // A window group's FS modes are considered to be owned by its current window
    if (const auto WINDOW_GROUP_TARGET = dc<Layout::CWindowGroupTarget*>(target.get()); WINDOW_GROUP_TARGET && target->type() == Layout::TARGET_TYPE_GROUP) {
        if (WINDOW_GROUP_TARGET->getGroup() && WINDOW_GROUP_TARGET->getGroup()->current() && WINDOW_GROUP_TARGET->getGroup()->current()->m_target)
            target = WINDOW_GROUP_TARGET->getGroup()->current()->m_target;
        else
            return FULLSCREEN_REQUEST_FAILED;
    }

    const auto& ITR = m_fsTargets.find(target);

    // not FS at all
    if (ITR == m_fsTargets.end())
        return false;

    return mode.has_value() ? ITR->second.internal == mode.value() : ITR->second.internal != FSMODE_NONE;
}

bool IFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {
    return std::ranges::any_of(m_fsTargets, [&](const auto& e) { return (e.first && e.second.internal != FSMODE_NONE); });
}

SP<Layout::ITarget> IFullscreenHandler::getFullscreen(const std::optional<bool> covering) {
    for (const auto& e : m_fsTargets) {
        if (e.first && e.second.internal != FSMODE_NONE)
            return e.first.lock();
    }
    return nullptr;
}

SFullscreenMode IFullscreenHandler::getFullscreenModes(SP<Layout::ITarget> target) {
    if (!target)
        return SFullscreenMode{};

    if (const auto WINDOW_GROUP_TARGET = dc<Layout::CWindowGroupTarget*>(target.get()); WINDOW_GROUP_TARGET && target->type() == Layout::TARGET_TYPE_GROUP) {
        if (WINDOW_GROUP_TARGET->getGroup() && WINDOW_GROUP_TARGET->getGroup()->current() && WINDOW_GROUP_TARGET->getGroup()->current()->m_target)
            target = WINDOW_GROUP_TARGET->getGroup()->current()->m_target;
        else
            return SFullscreenMode{};
    }

    const auto& ITR = m_fsTargets.find(target);

    return ITR == m_fsTargets.end() ? SFullscreenMode{} : ITR->second;
}

eFullscreenRequestResult IFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {
    if (!request.target || !request.target->window() || !request.target->workspace() || !request.target->workspace()->m_monitor)
        return FULLSCREEN_REQUEST_FAILED;

    const auto TARGET       = request.target;
    const auto GROUP_TARGET = request.target->window()->m_group ? request.target->window()->m_group->m_target : nullptr;
    const auto WINDOW       = TARGET->window();
    const auto WORKSPACE    = TARGET->workspace();
    const auto MONITOR      = WORKSPACE->m_monitor;

    setTargetFullscreenModeInternal(TARGET, request.mode);

    // save covering FS window if mode isn't FSMODE_NONE
    // If individual window, sets windowTarget's pos. If group, sets windowGroupTarget's pos - which will set all member target's positions in turn
    if (request.mode == FSMODE_FULLSCREEN) {
        const CBox MONBOX = MONITOR->logicalBox();
        (GROUP_TARGET ? GROUP_TARGET : TARGET)->setPositionGlobal(MONBOX);
    } else if (request.mode == FSMODE_MAXIMIZED) {
        const CBox WORKAREA = WORKSPACE->m_space->workArea(TARGET->floating());
        (GROUP_TARGET ? GROUP_TARGET : TARGET)->setPositionGlobal(WORKAREA);
    }

    setNoMembersAboveFullscreen();

    g_pDesktopAnimationManager->setFullscreenFadeAnimation(
        WORKSPACE, request.mode == FSMODE_NONE ? CDesktopAnimationManager::ANIMATION_TYPE_OUT : CDesktopAnimationManager::ANIMATION_TYPE_IN);

    return FULLSCREEN_REQUEST_DEFAULT_HANDLED;
}

void IFullscreenHandler::setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    if (!target)
        return;

    const auto ITR = m_fsTargets.find(target);

    // remember floating size if window exists and is floating
    if (target->window() && target->window()->m_isFloating && mode != FSMODE_NONE && !isFullscreen(target))
        target->rememberFloatingSize(target->position().size());

    if (mode == FSMODE_NONE) {
        if (ITR != m_fsTargets.end()) {
            ITR->second.internal = FSMODE_NONE;
        }
    } else if (ITR == m_fsTargets.end()) {
        m_fsTargets.emplace(target, SFullscreenMode{.internal = mode});
    } else {
        ITR->second.internal = mode;
    }

    syncFullscreenTargets();
}

void IFullscreenHandler::setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    const auto& ITR = m_fsTargets.find(target);

    if (mode == FSMODE_NONE) {
        if (ITR != m_fsTargets.end()) {
            ITR->second.client = FSMODE_NONE;
        }
    } else if (ITR == m_fsTargets.end()) {
        m_fsTargets.emplace(target, SFullscreenMode{.client = mode});
    } else {
        ITR->second.client = mode;
    }

    syncFullscreenTargets();
}

void IFullscreenHandler::setNoMembersAboveFullscreen() {
    if (!getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
        return;

    const auto SPACE     = getSpace();
    const auto WORKSPACE = SPACE->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor;

    const bool SET = hasFullscreen(true);

    // make all windows and layers on the same workspace under the fullscreen window
    for (auto const& w : Desktop::windowState()->windows()) {
        if (w && w->m_workspace == getSpace()->workspace() && !isFullscreen(w->m_target) && !w->m_pinned) {
            w->m_allowedOverFullscreen = !SET;
            w->updateFullscreenInputState();
        }
    }
    for (auto const& ls : Desktop::layerState()->layers()) {
        if (ls->m_monitor == MONITOR)
            ls->m_aboveFullscreen = !SET;
    }
}

void IFullscreenHandler::syncFullscreenTargets() {
    // Mode checking logic is the same as getFullscreenModes() - keep it in sync

    // to prevent a rehash - just in case
    std::vector<std::pair<WP<Layout::ITarget>, SFullscreenMode>> toInsert;

    for (auto it = m_fsTargets.begin(); it != m_fsTargets.end();) {
        // target expired
        // window doesn't exist
        // target is not FS (internal or client)
        if (!it->first || !it->first->window() || (!isFullscreen(it->first.lock()) && it->second.client == FSMODE_NONE)) {
            const auto NEXT = std::next(it);
            removeFsTarget(it->first.lock(), true);
            it = NEXT;
            continue;
        }

        // If ITarget's underlying type is CWindowGroupTarget; only store the current window, NOT the whole group
        // This should never have happened to begin with
        if (it->first->type() == Layout::TARGET_TYPE_GROUP) {
            const SFullscreenMode MODE         = SFullscreenMode{.internal = it->second.internal, .client = it->second.client};
            const auto            WINDOWTARGET = it->first->window()->layoutTarget();
            const auto            NEXT         = std::next(it);
            removeFsTarget(it->first.lock(), true);
            it = NEXT;
            toInsert.emplace_back(WINDOWTARGET, MODE);
            continue;
        }

        ++it;
    }

    for (const auto& e : toInsert) {
        m_fsTargets.emplace(e.first, e.second);
    }
}

void IFullscreenHandler::removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard) {
    const auto ITER = m_fsTargets.find(target);
    if (ITER != m_fsTargets.end())
        m_fsTargets.erase(ITER);

    if (!recursionGuard)
        syncFullscreenTargets();
}

eFullscreenHandler IFullscreenHandler::getFullscreenHandlerName() const {
    return FULLSCREEN_HANDLER_TYPE;
}

SP<Layout::CSpace> IFullscreenHandler::getSpace() const {
    if (!m_algorithm || !m_algorithm->m_parent || !m_algorithm->m_parent->space())
        return nullptr;
    return m_algorithm->m_parent.lock()->space();
}

SP<Layout::CAlgorithm> IFullscreenHandler::getParent() const {
    if (!m_algorithm || !m_algorithm->m_parent)
        return nullptr;

    return m_algorithm->m_parent.lock();
}
