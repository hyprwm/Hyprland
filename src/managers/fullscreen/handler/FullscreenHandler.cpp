
#include "FullscreenHandler.hpp"
#include "Compositor.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/DesktopTypes.hpp"
#include "desktop/view/LayerSurface.hpp"
#include "layout/algorithm/Algorithm.hpp"
#include "managers/animation/DesktopAnimationManager.hpp"
#include "managers/fullscreen/FullscreenController.hpp"
#include "output/Monitor.hpp"

using namespace Fullscreen;

IFullscreenHandler::IFullscreenHandler(Layout::IModeAlgorithm* algorithm) : m_algorithm(algorithm) {
    if (!m_algorithm) {
        Log::logger->log(Log::CRIT, "IFullscreenHandler failed during construction: Owning layout algorithm does not exist!");
    }
};

bool IFullscreenHandler::isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {

    if (mode.value() != FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen. This must never happpen.");
        return false;
    }

    const auto& WINITR = m_fsWindows.find(window);

    // not FS at all
    if (WINITR == m_fsWindows.end())
        return false;

    return mode.has_value() ? WINITR->second.internal == mode.value() : WINITR->second.internal != FSMODE_NONE;
}

bool IFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {

    for (const auto& e : m_fsWindows) {
        if (e.first && e.second.internal != FSMODE_NONE)
            return true;
    }

    return false;
}

PHLWINDOW IFullscreenHandler::getFullscreen(const std::optional<bool> covering) {

    for (const auto& e : m_fsWindows) {
        if (e.first && e.second.internal != FSMODE_NONE)
            return e.first.lock();
    }

    return nullptr;
}

SFullscreenMode IFullscreenHandler::getFullscreenMode(const PHLWINDOW window) {

    const auto& WINITR = m_fsWindows.find(window);

    // not FS at all
    if (WINITR == m_fsWindows.end())
        return SFullscreenMode{};

    return WINITR->second;
}

eFullscreenRequestResult IFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {
    if (!request.target || !request.target->window() || !request.target->workspace() || !request.target->workspace()->m_monitor)
        return FULLSCREEN_REQUEST_FAILED;

    const auto TARGET    = request.target;
    const auto WINDOW    = TARGET->window();
    const auto WORKSPACE = TARGET->workspace();
    const auto MONITOR   = WORKSPACE->m_monitor;

    setWindowFullscreenModeInternal(WINDOW, request.mode);

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

void IFullscreenHandler::setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode) {

    const auto& WINITR = m_fsWindows.find(window);

    if (window->m_isFloating && !isFullscreen(window))
        window->m_target->rememberFloatingSize(window->m_target->getPositionGlobal().logicalBox.size());

    if (mode == FSMODE_NONE) {
        if (WINITR != m_fsWindows.end()) {
            WINITR->second.internal = FSMODE_NONE;
        }
    } else if (WINITR == m_fsWindows.end()) {
        m_fsWindows.emplace(window, SFullscreenMode{.internal = mode});
    } else {
        WINITR->second.internal = mode;
    }

    syncFullscreenWindows();
}

void IFullscreenHandler::setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode) {

    const auto& WINITR = m_fsWindows.find(window);

    if (mode == FSMODE_NONE) {
        if (WINITR != m_fsWindows.end()) {
            WINITR->second.client = FSMODE_NONE;
        }
    } else if (WINITR == m_fsWindows.end()) {
        m_fsWindows.emplace(window, SFullscreenMode{.client = mode});
    } else {
        WINITR->second.client = mode;
    }

    syncFullscreenWindows();
}

void IFullscreenHandler::moveFullscreenWindowToHandler(const PHLWINDOW window, const std::optional<bool> covering) {

    // on hold for now.  maybe unFSing a window in one and FSing it again on the other workspace is the better choice
}

void IFullscreenHandler::moveFullscreenWindowOutOfHandler(const PHLWINDOW window) {

    // on hold for now. maybe unFSing a window in one and FSing it again on the other workspace is the better choice
}

void IFullscreenHandler::setNoMembersAboveFullscreen() {
    if (!getSpace() || !getSpace()->workspace() || !getSpace()->workspace()->m_monitor)
        return;

    const auto SPACE = getSpace();
    const auto WORKSPACE = SPACE->workspace();
    const auto MONITOR = WORKSPACE->m_monitor;

    const bool SET = !m_fsWindows.empty();

    // make all windows and layers on the same workspace under the fullscreen window
    for (const auto& e : WORKSPACE->getWindows()) {
        if (e && !isFullscreen(e) && !e->m_fadingOut && !e->m_pinned) {
            e->m_allowedOverFullscreen = !SET;
            e->updateFullscreenInputState();
        }
    }
    for (auto const& ls : g_pCompositor->m_layers) {
        if (ls->m_monitor == MONITOR)
            ls->m_aboveFullscreen = !SET;
    }
}

void IFullscreenHandler::syncFullscreenWindows() {

    // search for phlwindow null or mode both fsmodenone

    // serach for multiple internal != none. remove the latter found ones

    PHLWINDOW coveringFullscreenWindow;

    for (const auto& e : m_fsWindows) {
        // window expired
        // both modes are false
        if (!e.first || (e.second.internal == FSMODE_NONE && e.second.client == FSMODE_NONE)) {
            m_fsWindows.erase(e.first);
        }
    }
}

eFullscreenHandler IFullscreenHandler::getFullscreenHandlerName() const {
    return FULLSCREEN_HANDLER_TYPE;
}

SP<Layout::CSpace> IFullscreenHandler::getSpace() const {
    if (!m_algorithm || !m_algorithm->m_parent || m_algorithm->m_parent->space())
        return nullptr;
    return m_algorithm->m_parent.lock()->space();
}
