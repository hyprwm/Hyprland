
#include "FullscreenHandler.hpp"
#include "Compositor.hpp"
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



bool IFullscreenHandler::isFullscreen(const PHLWINDOW window, const std::optional<bool> covering) {

    const auto& WINPOSINLIST = m_fsWindows.find(window);

    // not FS at all
    if (WINPOSINLIST == m_fsWindows.end())
        return false;

    return WINPOSINLIST->second.internal != FSMODE_NONE;    
}


bool IFullscreenHandler::hasFullscreen(const std::optional<bool> covering) {


    for (const auto& w : m_fsWindows) {
        if (w.first && w.second.internal != FSMODE_NONE)
            return true;
    }

    return false;
}

PHLWINDOW IFullscreenHandler::getFullscreen(const std::optional<bool> covering) {


    for (const auto& w : m_fsWindows) {
        if (w.first && w.second.internal != FSMODE_NONE)
            return w.first.lock();
    }

    return nullptr;

}

SFullscreenMode IFullscreenHandler::getFullscreenMode(const PHLWINDOW window) {

    const auto& WINPOSINLIST = m_fsWindows.find(window);

    // not FS at all
    if (WINPOSINLIST == m_fsWindows.end())
        return SFullscreenMode{};

    return WINPOSINLIST->second;
}


eFullscreenRequestResult IFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {
    if (!request.target || !request.target->window() || !request.target->workspace() || !request.target->workspace()->m_monitor)
        return FULLSCREEN_REQUEST_FAILED;



    const auto TARGET = request.target;
    const auto WINDOW = TARGET->window();
    const auto WORKSPACE = TARGET->workspace();
    const auto MONITOR = WORKSPACE->m_monitor;

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

    const auto& WINPOSINLIST = m_fsWindows.find(window);

    if (mode == FSMODE_NONE) {
        if (WINPOSINLIST != m_fsWindows.end()) {
            WINPOSINLIST->second.internal = FSMODE_NONE;
        }
    }
    else if (WINPOSINLIST == m_fsWindows.end()) {
        m_fsWindows.emplace(window, SFullscreenMode{.internal = mode});
    }
    else {
        WINPOSINLIST->second.internal = mode;
    }

    syncFullscreenWindows();
}

void IFullscreenHandler::setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode) {

    const auto& WINPOSINLIST = m_fsWindows.find(window);

    if (mode == FSMODE_NONE) {
        if (WINPOSINLIST != m_fsWindows.end()) {
            WINPOSINLIST->second.client = FSMODE_NONE;
        }
    }
    else if (WINPOSINLIST == m_fsWindows.end()) {
        m_fsWindows.emplace(window, SFullscreenMode{.client = mode});
    }
    else {
        WINPOSINLIST->second.client = mode;
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

    const auto SPACE = getSpace();

    if (!SPACE)
        return;

    const auto WORKSPACE = SPACE->workspace();

    if (!WORKSPACE)
        return;

    const auto MONITOR = WORKSPACE->m_monitor;

    if (!MONITOR)
        return;


    const bool SET = !m_fsWindows.empty();

    // make all windows and layers on the same workspace under the fullscreen window
    for (const auto& w : WORKSPACE->getWindows()) {
        if (w && !isFullscreen(w) && !w->m_fadingOut && !w->m_pinned) {
            w->m_allowedOverFullscreen = !SET;
            w->updateFullscreenInputState();
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

    for (const auto& w : m_fsWindows) {
        // window expired
        // both modes are false
        if (!w.first || (w.second.internal == FSMODE_NONE && w.second.client == FSMODE_NONE)) {
            m_fsWindows.erase(w.first);
        }
    }

}



eFullscreenHandler IFullscreenHandler::getFullscreenHandlerName() const {
    return FULLSCREEN_HANDLER_TYPE;
}


SP<Layout::CSpace> IFullscreenHandler::getSpace()const {
    return m_algorithm->m_parent.lock()->space();
}
