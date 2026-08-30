#include "FullscreenHandler.hpp"
#include "../../../desktop/view/window/WindowFullscreenPolicy.hpp"
#include "../../../desktop/view/window/WindowPresentation.hpp"

#include "../../../managers/fullscreen/FullscreenController.hpp"

#include "../../../debug/log/Logger.hpp"

#include "../../../desktop/DesktopTypes.hpp"
#include "../../../desktop/view/LayerSurface.hpp"
#include "../../../desktop/view/window/Window.hpp"
#include "../../../desktop/view/window/WindowGroupMembership.hpp"
#include "../../../desktop/state/LayerState.hpp"
#include "../../../desktop/state/WindowState.hpp"

#include "../../../layout/algorithm/Algorithm.hpp"
#include "../../../layout/target/Target.hpp"
#include "../../../layout/target/WindowGroupTarget.hpp"
#include "../../../layout/target/WindowTarget.hpp"

#include "../../../render/Renderer.hpp"
#include "../../../animation/WorkspaceAnimationController.hpp"
#include "../../../output/Monitor.hpp"

#include "../../../config/shared/monitor/MonitorRuleManager.hpp"
#include <hyprutils/memory/Casts.hpp>

using namespace Fullscreen;

IFullscreenHandler::IFullscreenHandler(Layout::IModeAlgorithm* const algorithm) : m_algorithm(algorithm) {
    if (!m_algorithm)
        LOG(Log::CRIT, "IFullscreenHandler failed during construction: Owning layout algorithm does not exist!");
};

bool IFullscreenHandler::isFullscreen(SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    // Mode checking logic is the same as getFullscreenModes() - keep it in sync

    if (!target)
        return false;

    if (mode.value_or(FSMODE_FULLSCREEN) == FSMODE_NONE) {
        LOG(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen(). Negating the result instead");
        !isFullscreen(target, std::nullopt, covering);
    }

    // A window group's FS state is considered to be owned by its current window
    if (const auto WINDOW_GROUP_TARGET = dc<Layout::CWindowGroupTarget*>(target.get()); WINDOW_GROUP_TARGET && target->type() == Layout::TARGET_TYPE_GROUP) {
        if (WINDOW_GROUP_TARGET->getGroup() && WINDOW_GROUP_TARGET->getGroup()->current() && WINDOW_GROUP_TARGET->getGroup()->current()->windowTarget())
            target = WINDOW_GROUP_TARGET->getGroup()->current()->windowTarget();
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

    // A window group's FS modes are considered to be owned by its current window
    if (const auto WINDOW_GROUP_TARGET = dc<Layout::CWindowGroupTarget*>(target.get()); WINDOW_GROUP_TARGET && target->type() == Layout::TARGET_TYPE_GROUP) {
        if (WINDOW_GROUP_TARGET->getGroup() && WINDOW_GROUP_TARGET->getGroup()->current() && WINDOW_GROUP_TARGET->getGroup()->current()->windowTarget())
            target = WINDOW_GROUP_TARGET->getGroup()->current()->windowTarget();
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

    // Must run before setting pos/size so the values like gaps, border size, etc... are updated
    updateTargetRulesAndDecos(TARGET);

    setTargetSizeAndPosition(TARGET);

    setNoMembersAboveFullscreen();

    Animation::Workspace::setFullscreenFadeAnimation(WORKSPACE, request.mode != FSMODE_NONE ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT);

    /* Setting DS and VRR */

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

    if (target->window() && target->window()->isFloating() && mode != FSMODE_NONE && !isFullscreen(target))
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

void IFullscreenHandler::updateTargetRulesAndDecos(const SP<Layout::ITarget> target) {
    if (!target || !target->window() || !target->workspace() || !target->workspace()->m_monitor)
        return;

    const auto MONITOR = target->workspace()->m_monitor.lock();
    const auto WINDOW  = target->window();

    // Target must be a fullscreen window as considered by window/workspace rule matchers by now.
    // update all the values necessary for FS windows to get correct window dimensions and pos

    // If window is in a group, we need to update these values for ALL members of the group.
    if (WINDOW->grouping().group()) {
        for (const auto& gm : WINDOW->grouping().group()->windows()) {
            gm->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                    Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
            gm->presentation().updateDecorations();
        }
    } else {
        WINDOW->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                    Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        WINDOW->presentation().updateDecorations();
    }
    g_layoutManager->recalculateMonitor(MONITOR, Layout::CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_FULLSCREEN);
    getSpace()->recalculate(Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN);
}

void IFullscreenHandler::setTargetSizeAndPosition(const SP<Layout::ITarget> target) {
    if (!target || !target->window() || !target->workspace() || !target->workspace()->m_monitor)
        return;

    const auto TARGET_INTERNAL_MODE = getFullscreenModes(target).internal;

    // If not FS, let the layout's recalculate() handle window pos/size
    if (TARGET_INTERNAL_MODE == FSMODE_NONE)
        return;

    const auto WORKSPACE = target->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor.lock();

    // must set pos of the highest level target (i.e. if target a part of a group, must set that group's pos which will set the pos of all member targets)
    const auto LAYOUT_TARGET = target->window()->layoutTarget();

    if (TARGET_INTERNAL_MODE == FSMODE_FULLSCREEN) {
        const CBox MONBOX                                  = MONITOR->logicalBox();
        Fullscreen::controller()->m_windowPosSettingQueued = true;
        LAYOUT_TARGET->setPositionGlobal(MONBOX);
    } else if (TARGET_INTERNAL_MODE == FSMODE_MAXIMIZED) {
        const CBox WORKAREA                                = WORKSPACE->m_space->workArea(target->floating());
        Fullscreen::controller()->m_windowPosSettingQueued = true;
        LAYOUT_TARGET->setPositionGlobal(WORKAREA);
    }
    Fullscreen::controller()->m_windowPosSettingQueued = false;
}

void IFullscreenHandler::syncTargetSizeAndPosition() {

    // Expected Dimensions logic must be the same as updatePos(), and setting dimensions code must be the same as in setTargetSizeAndPosition(). Keep in sync

    const auto FS_TARGET            = getFullscreen(true);
    const auto TARGET_INTERNAL_MODE = getFullscreenModes(FS_TARGET).internal;

    // If not FS, let the layout's recalculate() handle window pos/size
    if (!FS_TARGET || !FS_TARGET->window() || !FS_TARGET->workspace() || !FS_TARGET->workspace()->m_monitor || TARGET_INTERNAL_MODE == FSMODE_NONE)
        return;

    const auto WORKSPACE = FS_TARGET->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor.lock();
    const auto WINDOW    = FS_TARGET->window();

    // must set pos of the highest level target (i.e. if target a part of a group, must set that group's pos which will set the pos of all member targets)
    const auto LAYOUT_TARGET = FS_TARGET->window()->layoutTarget();

    if (TARGET_INTERNAL_MODE == FSMODE_FULLSCREEN) {

        const CBox MONBOX         = MONITOR->logicalBox();
        const CBox ROUNDED_MONBOX = MONITOR->logicalBox().round();

        const auto CURRENT_REAL_POS_GOAL  = WINDOW.get()->position(Desktop::View::IGeometric::GEOMETRIC_GOAL);
        const auto CURRENT_REAL_SIZE_GOAL = WINDOW.get()->size(Desktop::View::IGeometric::GEOMETRIC_GOAL);

        const auto EXPECTED_REAL_POS  = ROUNDED_MONBOX.pos();
        const auto EXPECTED_REAL_SIZE = ROUNDED_MONBOX.size();

        // No need to compare with current value also since it'll get overwritten by goal anyway
        if (CURRENT_REAL_POS_GOAL != EXPECTED_REAL_POS || CURRENT_REAL_SIZE_GOAL != EXPECTED_REAL_SIZE) {
            controller()->m_windowPosSettingQueued = true;
            LAYOUT_TARGET->setPositionGlobal(MONBOX);
        }
    } else if (TARGET_INTERNAL_MODE == FSMODE_MAXIMIZED) {

        const auto WORK_AREA       = WORKSPACE->m_space->workArea(FS_TARGET->floating());
        auto       roundedWorkArea = WORK_AREA;
        roundedWorkArea.round();

        // Reserved area must be updated before this is called
        const auto RESERVED = WINDOW->getFullWindowReservedArea();

        const auto CURRENT_REAL_POS_GOAL  = WINDOW.get()->position(Desktop::View::IGeometric::GEOMETRIC_GOAL);
        const auto CURRENT_REAL_SIZE_GOAL = WINDOW.get()->size(Desktop::View::IGeometric::GEOMETRIC_GOAL);

        const auto EXPECTED_REAL_POS  = roundedWorkArea.pos() + RESERVED.topLeft;
        const auto EXPECTED_REAL_SIZE = roundedWorkArea.size() - (RESERVED.topLeft + RESERVED.bottomRight);

        if (CURRENT_REAL_POS_GOAL != EXPECTED_REAL_POS || CURRENT_REAL_SIZE_GOAL != EXPECTED_REAL_SIZE) {
            controller()->m_windowPosSettingQueued = true;
            LAYOUT_TARGET->setPositionGlobal(WORKSPACE->m_space->workArea(FS_TARGET->floating()));
        }
    }
    Fullscreen::controller()->m_windowPosSettingQueued = false;
}

void IFullscreenHandler::setNoMembersAboveFullscreen(const std::optional<SP<Layout::ITarget>> coveringFsTarget) {
    if (!getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
        return;

    const auto SPACE     = getSpace();
    const auto WORKSPACE = SPACE->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor;

    const bool SET = hasFullscreen(true);

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w && w->m_workspace == getSpace()->workspace() && !isFullscreen(w->windowTarget()) && !(w->m_state & Desktop::View::WINDOW_STATE_PINNED)) {
            w->fullscreenPolicy().setAllowedOverFullscreen(!SET);
            w->updateFullscreenInputState();
        }
    }
    for (auto const& ls : Desktop::layerState()->layers()) {
        if (ls->m_monitor != MONITOR)
            continue;

        ls->m_flags &= ~Desktop::View::LAYER_FLAG_ABOVE_FULLSCREEN;
    }
}

void IFullscreenHandler::syncFullscreenTargets() {
    // Mode checking logic is the same as getFullscreenModes() - keep it in sync



    const auto removeTargetFromList = [&](auto& it){
            const auto NEXT = std::next(it);
            m_fsTargets.erase(it);
            it = NEXT;
    };

    const auto removeFsTargetAndreturnNextIter = [&](auto& it, const auto& TARGET) {
            const auto NEXT = std::next(it);
            removeFsTarget(TARGET, true);
            return NEXT;
    };


    
    decltype(m_fsTargets) keep = {};
    keep.reserve(m_fsTargets.size());




    for (auto it = m_fsTargets.begin(); it != m_fsTargets.end();) {

        const auto TARGET = it->first.lock();

        // expired/invalid entries. We need not perform any post-removal operations on these
        if (!TARGET || !TARGET->window()) {
            removeTargetFromList(it);
            continue;
        }

        if (!isFullscreen(TARGET) && it->second.client == FSMODE_NONE) {
            it = removeFsTargetAndreturnNextIter(it, TARGET);
            continue;
        }

        // If ITarget's underlying type is CWindowGroupTarget; only store the current window, NOT the whole group
        if (TARGET->type() == Layout::TARGET_TYPE_GROUP) {
            const SFullscreenMode MODE = SFullscreenMode{.internal = it->second.internal, .client = it->second.client};
            // gets the current window's target in the window group
            const auto WINDOWTARGET = TARGET->window()->windowTarget();

            if (!WINDOWTARGET) {
                removeTargetFromList(it);
                continue;
            }
            
            if (!isFullscreen(WINDOWTARGET) && MODE.client == FSMODE_NONE) {
                it = removeFsTargetAndreturnNextIter(it, TARGET);
                continue;                
            }
            // do post-remove-ops on the window group target as that's what algorithms store as a target, then save only the current window of the group
            it = removeFsTargetAndreturnNextIter(it, TARGET);
            keep.emplace(WINDOWTARGET, MODE);
            continue;
        }
        keep.emplace(it->first, it->second);
        ++it;
    }

    m_fsTargets.swap(keep);
}

void IFullscreenHandler::removeFsTarget(SP<Layout::ITarget> target, const bool recursionGuard) {

    // This should not be done because if there are 2 expired elements in the list, this will remove one of them with little guarantee as to which and the caller is preumsably looking forward and getting the next
    // iterator in the list; which would corrupt the map if that next iter is also nullptr. We let this go through on the offchance that there is Ǝ! expired entries or the erase hits the right expired target but it is
    // non-deterministic
    if (!target)
        Log::logger->log(Log::CRIT, "IFullscreenHandler::removeFsTarget() called with target = nullptr. This is possibly non-deterministic and should NOT happen. This is a bug and should be reported!");

    m_fsTargets.erase(target);

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
