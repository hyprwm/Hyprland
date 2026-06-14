
#include "FullscreenHandler.hpp"



using namespace Fullscreen;


bool IFullscreenHandler::isFullscreen(const PHLWINDOW window, const std::optional<bool> covering) {

    // covering is not used in default FS handler - the unique FS window in a workspace has to be covering in default FS behaviour.
    return window == m_fullscreenWindow;
};


bool IFullscreenHandler::hasCoveringFullscreen() {

    // FS window has to be covering with default FS behaviour
    return m_fullscreenWindow;

};

PHLWINDOW IFullscreenHandler::getCoveringFullscreen() {

    return m_fullscreenWindow.lock();
};




eFullscreenRequestResult IFullscreenHandler::requestFullscreen(const SFullscreenRequest& request) {


};


void IFullscreenHandler::moveFullscreenWindowToHandler(const PHLWINDOW window) {

    // on hold for now.  maybe unFSing a window in one and FSing it again on the other workspace is the better choice

};

void IFullscreenHandler::moveCoveringFullscreenWindowToHandler(const PHLWINDOW window) {

    // on hold for now.  maybe unFSing a window in one and FSing it again on the other workspace is the better choice


};

void IFullscreenHandler::moveFullscreenWindowOutOfHandler(const PHLWINDOW window) {

    // on hold for now. maybe unFSing a window in one and FSing it again on the other workspace is the better choice


};


void IFullscreenHandler::setNoMembersAboveFullscreen() {


};

void IFullscreenHandler::syncFullscreenTargets() {
    ;
};


eFullscreenHandler IFullscreenHandler::getFullscreenHandlerName() const {


}; // simply return FULLSCREEN_HANDLER_TYPE

