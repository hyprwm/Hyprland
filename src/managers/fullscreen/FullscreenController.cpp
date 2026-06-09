
#include "FullscreenController.hpp"
#include "desktop/view/Window.hpp"

using namespace Fullscreen;




void CFullscreenController::setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode) {

    const auto found = m_fullscreenWindows.find(window);

    if (found == m_fullscreenWindows.end()) {
        m_fullscreenWindows.emplace(window, SWindowFullscreenState{ .mode={ .internal=FSMODE_NONE, .client=mode }, .fullscreenHandler=Desktop::View::FULLSCREEN_HANDLER_NONE });
    }
    else {
        found->second.mode.client = mode;
    }

}


void CFullscreenController::setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode) {


    const auto found = m_fullscreenWindows.find(window);

    if (found == m_fullscreenWindows.end()) {
        m_fullscreenWindows.emplace(window, SWindowFullscreenState{ .mode={ .internal=mode, .client=FSMODE_NONE }, .fullscreenHandler=Desktop::View::FULLSCREEN_HANDLER_NONE });
    }
    else {
        found->second.mode.internal = mode;
    }

}



SFullscreenMode CFullscreenController::getFullscreenMode(const PHLWINDOW window) {

    const auto found = m_fullscreenWindows.find(window);

    if (found == m_fullscreenWindows.end()) {
        return SFullscreenMode{.internal = FSMODE_NONE, .client = FSMODE_NONE};
    }
    else {
        return found->second.mode;
    }

}


SFullscreenMode CFullscreenController::getCoveringFullscreenMode(const PHLWORKSPACE workspace) {

}

















