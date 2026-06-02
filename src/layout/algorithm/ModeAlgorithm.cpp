#include "ModeAlgorithm.hpp"

#include "../space/Space.hpp"
#include "Algorithm.hpp"
#include "../../output/Monitor.hpp"
#include "../../desktop/view/Window.hpp"
#include "Compositor.hpp"
#include "desktop/Workspace.hpp"
#include "desktop/view/LayerSurface.hpp"
#include "layout/LayoutManager.hpp"
#include "managers/animation/DesktopAnimationManager.hpp"

using namespace Layout;

Config::ErrorResult IModeAlgorithm::layoutMsg(const std::string_view& sv) {
    return {};
}

std::optional<Vector2D> IModeAlgorithm::predictSizeForNewTarget() {
    return std::nullopt;
}

eFullscreenRequestResult IModeAlgorithm::requestFullscreen(const SFullscreenRequest& request) {


    // Default handled fullscreen behaviour

    const auto TARGET = request.target;

    if (!TARGET)
        return FULLSCREEN_REQUEST_FAILED;


    if (request.effectiveMode != FSMODE_NONE && TARGET->window())
        TARGET->window()->m_fullscreenHandler = Desktop::View::FULLSCREEN_HANDLER_DEFAULT;
    else
        TARGET->window()->m_fullscreenHandler = Desktop::View::FULLSCREEN_HANDLER_NONE;


    // set internal fullscreen mode
    TARGET->setFullscreenMode(request.effectiveMode);

    const auto TARGETWORKSPACE = TARGET->workspace();

    // set workspace fullscreen attributes
    if (TARGETWORKSPACE) {
        TARGETWORKSPACE->m_hasFullscreenWindow = request.effectiveMode != FSMODE_NONE;
        TARGETWORKSPACE->m_fullscreenMode = request.effectiveMode;
    }
    else
        return FULLSCREEN_REQUEST_FAILED;


        
    // Set window positions
    if (!TARGETWORKSPACE)
        return FULLSCREEN_REQUEST_FAILED;

    const auto MONITOR = TARGETWORKSPACE->m_monitor;
    if (!MONITOR)
        return FULLSCREEN_REQUEST_FAILED;

    if (request.effectiveMode == FSMODE_FULLSCREEN) {
        const CBox MONBOX = TARGETWORKSPACE->m_monitor->logicalBox();
        TARGET->setPositionGlobal(MONBOX); // TODO ERSTARR: This should work, unless logical box isn't what i'm looking for; then, just make a monitor box manually
    }
    else if (request.effectiveMode == FSMODE_MAXIMIZED) {
        const CBox WORKAREA = TARGETWORKSPACE->m_space->workArea(TARGET->floating());
        TARGET->setPositionGlobal(WORKAREA);
    }

    g_pDesktopAnimationManager->setFullscreenFadeAnimation(TARGETWORKSPACE, request.effectiveMode == FSMODE_NONE ? CDesktopAnimationManager::ANIMATION_TYPE_OUT : CDesktopAnimationManager::ANIMATION_TYPE_IN);


    IModeAlgorithm::setNoMembersAboveFullscreen(TARGET, request.effectiveMode != FSMODE_NONE);


    return FULLSCREEN_REQUEST_DEFAULT;
}

SP<ITarget> IModeAlgorithm::layoutFullscreenTarget() const {
    return nullptr;
}



void IModeAlgorithm::setNoMembersAboveFullscreen(SP<ITarget> fullscreenTarget, bool set) const {

    const auto WORKSPACE = fullscreenTarget->workspace();
    
    if (!WORKSPACE)
        return;

    const auto MONITOR = fullscreenTarget->workspace()->m_monitor;

    if (!MONITOR)
        return;


    // Get the currently fullscreen window in the current workspace if not explicitly given
    const auto FULLSCREEN_WINDOW =  WORKSPACE->getFullscreenWindow();

    if (!FULLSCREEN_WINDOW)
        return;


    if (set) {
        // make all windows and layers on the same workspace under the fullscreen window
        for (auto const& w : g_pCompositor->m_windows) {
            if (w->m_workspace == WORKSPACE && w != FULLSCREEN_WINDOW && !w->m_fadingOut && !w->m_pinned) {
                w->m_allowedOverFullscreen = false;
            }
            w->updateFullscreenInputState();
        }
        for (auto const& ls : g_pCompositor->m_layers) {
            if (ls->m_monitor == MONITOR)
            ls->m_aboveFullscreen = false;
        }
    }
}





std::optional<Vector2D> IModeAlgorithm::focalPointForDir(SP<ITarget> t, Math::eDirection dir) {
    Vector2D   focalPoint;

    const auto getFullscreenBB = [&]() -> std::optional<CBox> {
        const auto PARENT = m_parent.lock();
        if (!PARENT)
            return std::nullopt;
        const auto SPACE = PARENT->space();
        if (!SPACE)
            return std::nullopt;
        const auto WS = SPACE->workspace();
        if (!WS || !WS->m_monitor)
            return std::nullopt;
        return WS->m_monitor->logicalBox();
    };

    const auto WINDOWIDEALBB = t->fullscreenMode() != FSMODE_NONE ? getFullscreenBB().value_or(t->window()->getWindowIdealBoundingBoxIgnoreReserved()) :
                                                                    t->window()->getWindowIdealBoundingBoxIgnoreReserved();

    switch (dir) {
        case Math::DIRECTION_UP: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x / 2.0, -1.0}; break;
        case Math::DIRECTION_DOWN: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x / 2.0, WINDOWIDEALBB.size().y + 1.0}; break;
        case Math::DIRECTION_LEFT: focalPoint = WINDOWIDEALBB.pos() + Vector2D{-1.0, WINDOWIDEALBB.size().y / 2.0}; break;
        case Math::DIRECTION_RIGHT: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x + 1.0, WINDOWIDEALBB.size().y / 2.0}; break;
        default: return std::nullopt;
    }

    return focalPoint;
}
