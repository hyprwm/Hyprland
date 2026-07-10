#include "FullscreenHandler.hpp"

#include "../../../managers/fullscreen/FullscreenController.hpp"

#include "../../../debug/log/Logger.hpp"

#include "../../../desktop/DesktopTypes.hpp"
#include "../../../desktop/view/LayerSurface.hpp"
#include "../../../desktop/state/LayerState.hpp"
#include "../../../desktop/state/WindowState.hpp"

#include "../../../layout/algorithm/Algorithm.hpp"
#include "../../../layout/target/Target.hpp"
#include "../../../layout/target/WindowGroupTarget.hpp"

#include "../../../render/Renderer.hpp"
#include "../../../animation/WorkspaceAnimationController.hpp"
#include "../../../output/Monitor.hpp"

#include "../../../config/shared/monitor/MonitorRuleManager.hpp"
#include <hyprutils/memory/Casts.hpp>

using namespace Fullscreen;

IFullscreenHandler::IFullscreenHandler(Layout::IModeAlgorithm* const algorithm) : m_algorithm(algorithm) {
    if (!m_algorithm)
        Log::logger->log(Log::CRIT, "IFullscreenHandler failed during construction: Owning layout algorithm does not exist!");
};

bool IFullscreenHandler::isFullscreen(SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    // Mode checking logic is the same as getFullscreenModes() - keep it in sync

    if (!target)
        return false;

    if (mode.value_or(FSMODE_FULLSCREEN) == FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen(). Negating the result instead");
        !isFullscreen(target, std::nullopt, covering);
    }

    // A window group's FS modes are considered to be owned by its current window
    if (const auto WINDOW_GROUP_TARGET = dc<Layout::CWindowGroupTarget*>(target.get()); WINDOW_GROUP_TARGET && target->type() == Layout::TARGET_TYPE_GROUP) {
        if (WINDOW_GROUP_TARGET->getGroup() && WINDOW_GROUP_TARGET->getGroup()->current() && WINDOW_GROUP_TARGET->getGroup()->current()->m_target)
            target = WINDOW_GROUP_TARGET->getGroup()->current()->m_target;
        else
            return false;
    }

    const auto& ITR = m_fsTargets.find(target);

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
        return {};

    if (const auto WINDOW_GROUP_TARGET = dc<Layout::CWindowGroupTarget*>(target.get()); WINDOW_GROUP_TARGET && target->type() == Layout::TARGET_TYPE_GROUP) {
        if (WINDOW_GROUP_TARGET->getGroup() && WINDOW_GROUP_TARGET->getGroup()->current() && WINDOW_GROUP_TARGET->getGroup()->current()->m_target)
            target = WINDOW_GROUP_TARGET->getGroup()->current()->m_target;
        else
            return {};
    }

    const auto ITR = m_fsTargets.find(target);

    if (ITR == m_fsTargets.end())
        return {};
    return ITR->second;
}

eFullscreenRequestResult IFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {
    if (!request.target || !request.target->window() || !request.target->workspace() || !request.target->workspace()->m_monitor)
        return FULLSCREEN_REQUEST_FAILED;

    const auto TARGET    = request.target;
    const auto WINDOW    = TARGET->window();
    const auto WORKSPACE = TARGET->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor.lock();

    setTargetFullscreenModeInternal(TARGET, request.mode);

    setTargetSizeAndPosition(TARGET);

    setNoMembersAboveFullscreen();

    Animation::Workspace::setFullscreenFadeAnimation(WORKSPACE, request.mode != FSMODE_NONE ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);

    // send a regular tranche if we are exiting fullscreen.
    // ignore if DS is disabled.
    static auto PDIRECTSCANOUT = CConfigValue<Config::INTEGER>("render:direct_scanout");

    if (*PDIRECTSCANOUT == 1 || (*PDIRECTSCANOUT == 2 && WINDOW->getContentType() == NContentType::CONTENT_TYPE_GAME)) {
        auto surf = WINDOW->getSolitaryResource();
        if (surf)
            g_pHyprRenderer->setSurfaceScanoutMode(surf, request.mode != FSMODE_NONE ? MONITOR : nullptr);
    }
    Config::monitorRuleMgr()->ensureVRR(MONITOR);

    return FULLSCREEN_REQUEST_DEFAULT_HANDLED;
}

void IFullscreenHandler::setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    if (!target)
        return;

    const auto ITR = m_fsTargets.find(target);

    if (target->window() && target->window()->m_isFloating && mode != FSMODE_NONE && !isFullscreen(target))
        target->rememberFloatingSize(target->position().size());

    if (mode == FSMODE_NONE) {
        if (ITR != m_fsTargets.end())
            ITR->second.internal = FSMODE_NONE;
    } else if (ITR == m_fsTargets.end())
        m_fsTargets.emplace(target, SFullscreenMode{.internal = mode});
    else
        ITR->second.internal = mode;

    syncFullscreenTargets();
}

void IFullscreenHandler::setTargetFullscreenModeClient(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    const auto& ITR = m_fsTargets.find(target);

    if (mode == FSMODE_NONE) {
        if (ITR != m_fsTargets.end())
            ITR->second.client = FSMODE_NONE;
    } else if (ITR == m_fsTargets.end())
        m_fsTargets.emplace(target, SFullscreenMode{.client = mode});
    else
        ITR->second.client = mode;

    syncFullscreenTargets();
}

void IFullscreenHandler::setTargetSizeAndPosition(const SP<Layout::ITarget> target) {

    if (!target->window())
        return;

    // must set pos of the highest level target (i.e. if target a part of a group, must set that group's pos which will set the pos of all member targets)
    const auto LAYOUT_TARGET = target->window()->layoutTarget();
    const auto WINDOW        = LAYOUT_TARGET->window();
    const auto WORKSPACE     = LAYOUT_TARGET->workspace();
    const auto MONITOR       = WORKSPACE->m_monitor.lock();

    const auto TARGET_INTERNAL_MODE = getFullscreenModes(target).internal;

    // Target must be a fullscreen window as considered by window/workspace rule matchers by now.
    // update all the values necessary for FS windows to get correct window dimensions and pos
    WINDOW->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    WINDOW->updateDecorationValues();
    g_layoutManager->recalculateMonitor(MONITOR, Layout::CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_FULLSCREEN);
    getSpace()->recalculate(Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN);

    if (TARGET_INTERNAL_MODE == FSMODE_FULLSCREEN) {
        const CBox MONBOX                                  = MONITOR->logicalBox();
        Fullscreen::controller()->m_windowPosSettingQueued = true;
        LAYOUT_TARGET->setPositionGlobal(MONBOX);
    } else if (TARGET_INTERNAL_MODE == FSMODE_MAXIMIZED) {
        const CBox WORKAREA                                = WORKSPACE->m_space->workArea(LAYOUT_TARGET->floating());
        Fullscreen::controller()->m_windowPosSettingQueued = true;
        LAYOUT_TARGET->setPositionGlobal(WORKAREA);
    }

    // If not FS, let the layout's recalculate() handle window pos/size
}

void IFullscreenHandler::setNoMembersAboveFullscreen() {
    if (!getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
        return;

    const auto SPACE     = getSpace();
    const auto WORKSPACE = SPACE->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor;

    const bool SET = hasFullscreen(true);

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

    // to prevent a rehash
    std::vector<std::pair<WP<Layout::ITarget>, SFullscreenMode>> toInsert;

    for (auto it = m_fsTargets.begin(); it != m_fsTargets.end();) {

        // Somehow happens sometimes and causes WP<> to segfault
        if (m_fsTargets.empty())
            return;

        // Rigorously check if WP<> is valid as WP<> randomly segfaults sometimes without this
        const auto TARGET = !it->first.expired() && it->first.valid() && it->first ? it->first.lock() : nullptr;

        if (!TARGET || !TARGET->window() || (!isFullscreen(TARGET) && it->second.client == FSMODE_NONE)) {
            const auto NEXT = std::next(it);
            removeFsTarget(TARGET, true);
            it = NEXT;
            continue;
        }

        // If ITarget's underlying type is CWindowGroupTarget; only store the current window, NOT the whole group
        if (TARGET->type() == Layout::TARGET_TYPE_GROUP) {
            const SFullscreenMode MODE         = SFullscreenMode{.internal = it->second.internal, .client = it->second.client};
            const auto            WINDOWTARGET = TARGET->window()->layoutTarget();
            const auto            NEXT         = std::next(it);
            removeFsTarget(TARGET, true);
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
