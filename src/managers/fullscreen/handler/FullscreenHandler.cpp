
#include "FullscreenHandler.hpp"
#include "Compositor.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/DesktopTypes.hpp"
#include "desktop/view/LayerSurface.hpp"
#include "layout/algorithm/Algorithm.hpp"
#include "layout/target/Target.hpp"
#include "managers/animation/DesktopAnimationManager.hpp"
#include "managers/fullscreen/FullscreenController.hpp"
#include "output/Monitor.hpp"
#include <algorithm>
#include <iterator>

using namespace Fullscreen;

IFullscreenHandler::IFullscreenHandler(Layout::IModeAlgorithm* algorithm) : m_algorithm(algorithm) {
    if (!m_algorithm) {
        Log::logger->log(Log::CRIT, "IFullscreenHandler failed during construction: Owning layout algorithm does not exist!");
        throw std::runtime_error("CScrollingFullscreenHandler: bad algorithm type");
    }
};

bool IFullscreenHandler::isFullscreen(const SP<Layout::ITarget> target, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {

    if (mode.value() != FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen. This must never happpen.");
        return false;
    }

    const auto& ITR = m_fsTargets.find(target);

    // not FS at all
    if (ITR == m_fsTargets.end())
        return false;

    return mode.has_value() ? ITR->second.internal == mode.value() : ITR->second.internal != FSMODE_NONE;
}

bool IFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {
    return std::ranges::any_of(m_fsTargets,[&](const auto& e){return (e.first && e.second.internal != FSMODE_NONE);});
}

SP<Layout::ITarget> IFullscreenHandler::getFullscreen(const std::optional<bool> covering) {
    for (const auto& e : m_fsTargets) {
        if (e.first && e.second.internal != FSMODE_NONE)
            return e.first.lock();
    }
    return nullptr;
}

SFullscreenMode IFullscreenHandler::getFullscreenMode(const SP<Layout::ITarget> target) {
    const auto& ITR = m_fsTargets.find(target);
    return ITR == m_fsTargets.end() ? SFullscreenMode{} : ITR->second;
}

eFullscreenRequestResult IFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {
    if (!request.target || !request.target->window() || !request.target->workspace() || !request.target->workspace()->m_monitor)
        return FULLSCREEN_REQUEST_FAILED;

    const auto TARGET    = request.target;
    const auto WINDOW    = TARGET->window();
    const auto WORKSPACE = TARGET->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor;

    setTargetFullscreenModeInternal(TARGET, request.mode);

    // save covering FS window if mode isn't FSMODE_NONE
    // set window size and pos
    if (request.mode == FSMODE_FULLSCREEN) {
        const CBox MONBOX = MONITOR->logicalBox();
        TARGET->setPositionGlobal(MONBOX);
    } else if (request.mode == FSMODE_MAXIMIZED) {
        const CBox WORKAREA = WORKSPACE->m_space->workArea(TARGET->floating());
        TARGET->setPositionGlobal(WORKAREA);
    }

    setNoMembersAboveFullscreen();

    g_pDesktopAnimationManager->setFullscreenFadeAnimation(
        WORKSPACE, request.mode == FSMODE_NONE ? CDesktopAnimationManager::ANIMATION_TYPE_OUT : CDesktopAnimationManager::ANIMATION_TYPE_IN);

    return FULLSCREEN_REQUEST_DEFAULT;
}

void IFullscreenHandler::setTargetFullscreenModeInternal(const SP<Layout::ITarget> target, const eFullscreenMode mode) {

    if (!target)
        return;

    const auto ITR = m_fsTargets.find(target);

    // remember floating size if window exists and is floating
    if (target->window() && target->window()->m_isFloating && !isFullscreen(target))
        target->rememberFloatingSize(target->getPositionGlobal().logicalBox.size());

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

void IFullscreenHandler::moveFullscreenTargetToHandler(const SP<Layout::ITarget> target, const std::optional<bool> covering) {
    // on hold for now.  maybe unFSing a window in one and FSing it again on the other workspace is the better choice
}

void IFullscreenHandler::moveFullscreenTargetOutOfHandler(const SP<Layout::ITarget> target) {

    // on hold for now. maybe unFSing a window in one and FSing it again on the other workspace is the better choice
}

void IFullscreenHandler::setNoMembersAboveFullscreen() {
    if (!getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
        return;

    const auto SPACE = getSpace();
    const auto WORKSPACE = SPACE->workspace();
    const auto MONITOR = WORKSPACE->m_monitor;

    const bool SET = !m_fsTargets.empty();

    // make all windows and layers on the same workspace under the fullscreen window
    for (const auto& e : WORKSPACE->getWindows()) {
        if (e && !isFullscreen(e->m_target) && !e->m_fadingOut && !e->m_pinned) {
            e->m_allowedOverFullscreen = !SET;
            e->updateFullscreenInputState();
        }
    }
    for (auto const& ls : g_pCompositor->m_layers) {
        if (ls->m_monitor == MONITOR)
            ls->m_aboveFullscreen = !SET;
    }
}

void IFullscreenHandler::syncFullscreenTargets() {


    // to prevent a rehash - just in case
    std::vector<std::pair<WP<Layout::ITarget>, SFullscreenMode>> toInsert;


    for (auto it = m_fsTargets.begin(); it != m_fsTargets.end(); ) {        
        // target exipred
        // window doesn't exist
        // both modes are false
        if (!it->first || !it->first->window() || (it->second.internal == FSMODE_NONE && it->second.client == FSMODE_NONE)) {
            const auto NEXT = std::next(it);
            removeFsTarget(it->first.lock());
            it = NEXT;
            continue;
        }

        // If ITarget's underlying type is CWindowGroupTarget; only store the current window, NOT the whole group
        // This should never have happened to begin with
        if (it->first->type() == Layout::TARGET_TYPE_GROUP) {
            const SFullscreenMode MODE = SFullscreenMode{.internal = it->second.internal, .client = it->second.client};
            const auto WINDOWTARGET = it->first->window()->layoutTarget();
            const auto NEXT = std::next(it);
            removeFsTarget(it->first.lock());
            it = NEXT;
            toInsert.emplace_back(WINDOWTARGET,MODE);
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
    if (!m_algorithm || !m_algorithm->m_parent || m_algorithm->m_parent->space())
        return nullptr;
    return m_algorithm->m_parent.lock()->space();
}
