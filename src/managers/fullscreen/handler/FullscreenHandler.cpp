
#include "FullscreenHandler.hpp"
#include "Compositor.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/view/LayerSurface.hpp"
#include "desktop/view/Window.hpp"
#include "layout/algorithm/ModeAlgorithm.hpp"
#include "layout/algorithm/Algorithm.hpp"
#include "layout/space/Space.hpp"
#include "managers/animation/DesktopAnimationManager.hpp"
#include "managers/fullscreen/FullscreenController.hpp"
#include "output/Monitor.hpp"
#include <cstddef>
#include <cstdlib>


using namespace Fullscreen;


IFullscreenHandler::IFullscreenHandler(Layout::IModeAlgorithm* algorithm) : m_algorithm(algorithm) {
    if (!m_algorithm) {
        Log::logger->log(Log::CRIT, "IFullscreenHandler failed during construction: Owning layout algorithm does not exist!");      
        abort();
    }
};



bool IFullscreenHandler::isFullscreen(const PHLWINDOW window, const std::optional<bool> covering) {
    return window == m_fullscreenWindow.window.lock() && m_fullscreenWindow.mode.internal != FSMODE_NONE;
}


bool IFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {
    return m_fullscreenWindow.window.lock() && m_fullscreenWindow.mode.internal != FSMODE_NONE;
}

PHLWINDOW IFullscreenHandler::getFullscreen(const std::optional<bool> covering) {
    return hasFullscreen() ? m_fullscreenWindow.window.lock() : nullptr;
}




eFullscreenRequestResult IFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {
    if (!request.target || !request.target->window() || !request.target->workspace() || !request.target->workspace()->m_monitor)
        return FULLSCREEN_REQUEST_FAILED;



    const auto TARGET = request.target;
    const auto WINDOW = TARGET->window();
    const auto WORKSPACE = TARGET->workspace();
    const auto MONITOR = WORKSPACE->m_monitor;


    // ERSTARR TODO NOW  move the FS functions from all over to here

    // set internal fullscreen mode

    setWindowFullscreenModeInternal(WINDOW, request.mode);

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


    if (window != m_fullscreenWindow.window)
        return;

    if (mode == FSMODE_NONE)
        removeCurrentFullscreenWindow();
    else
        m_fullscreenWindow.mode.internal = mode;

}

void IFullscreenHandler::setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode) {

    if (window != m_fullscreenWindow.window)
        return;

    if (mode == FSMODE_NONE)
        removeCurrentFullscreenWindow();
    else
        m_fullscreenWindow.mode.client = mode;
}




void IFullscreenHandler::moveFullscreenWindowToHandler(const PHLWINDOW window, const std::optional<bool> covering) {

    // on hold for now.  maybe unFSing a window in one and FSing it again on the other workspace is the better choice

}


void IFullscreenHandler::moveFullscreenWindowOutOfHandler(const PHLWINDOW window) {

    // on hold for now. maybe unFSing a window in one and FSing it again on the other workspace is the better choice


}


void IFullscreenHandler::setNoMembersAboveFullscreen() {

    const auto SPACE = getSpace();

    if (!SPACE)
        return;

    const auto WORKSPACE = SPACE->workspace();

    if (!WORKSPACE)
        return;

    const auto MONITOR = WORKSPACE->m_monitor;

    if (!MONITOR)
        return;


    const bool SET = m_fullscreenWindow.window;

    // make all windows and layers on the same workspace under the fullscreen window
    for (const auto& w : WORKSPACE->getWindows()) {
        if (w != m_fullscreenWindow.window && !w->m_fadingOut && !w->m_pinned) {
            w->m_allowedOverFullscreen = !SET;
            w->updateFullscreenInputState();
        }
    }
    for (auto const& ls : g_pCompositor->m_layers) {
        if (ls->m_monitor == MONITOR)
            ls->m_aboveFullscreen = !SET;
    }
}

void IFullscreenHandler::syncFullscreenTargets() {
    ;
}


eFullscreenHandler IFullscreenHandler::getFullscreenHandlerName() const {
    return FULLSCREEN_HANDLER_TYPE;
}


const Layout::IModeAlgorithm* IFullscreenHandler::getAlgorithm() const {
    return m_algorithm;
}

SP<Layout::CSpace> IFullscreenHandler::getSpace() const {
    return m_algorithm->m_parent.lock()->space();
}


void IFullscreenHandler::removeCurrentFullscreenWindow() {
    m_fullscreenWindow = {.window=nullptr, .mode={.internal=FSMODE_NONE, .client=FSMODE_NONE}};
}
