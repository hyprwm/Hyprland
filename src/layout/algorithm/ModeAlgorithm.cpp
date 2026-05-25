#include "ModeAlgorithm.hpp"

#include "../space/Space.hpp"
#include "Algorithm.hpp"
#include "../../output/Monitor.hpp"
#include "../../desktop/view/Window.hpp"
#include "desktop/Workspace.hpp"
#include "layout/LayoutManager.hpp"

using namespace Layout;

Config::ErrorResult IModeAlgorithm::layoutMsg(const std::string_view& sv) {
    return {};
}

std::optional<Vector2D> IModeAlgorithm::predictSizeForNewTarget() {
    return std::nullopt;
}

eFullscreenRequestResult IModeAlgorithm::requestFullscreen(const SFullscreenRequest& request) {

    const auto TARGET = request.target;

    if (!TARGET)
        return FULLSCREEN_REQUEST_FAILED;


    if (request.effectiveMode != FSMODE_NONE && TARGET->window())
        TARGET->window()->m_fullscreenHandler = Desktop::View::FULLSCREEN_HANDLER_DEFAULT;
    else
        TARGET->window()->m_fullscreenHandler = Desktop::View::FULLSCREEN_HANDLER_NONE;


    // set internal fullscreen mode
    TARGET->setFullscreenMode(request.effectiveMode);

    // set workspace fullscreen attributes
    if (const auto TARGETWORKSPACE = TARGET->workspace(); TARGETWORKSPACE) {
        TARGETWORKSPACE->m_hasFullscreenWindow = request.effectiveMode != FSMODE_NONE;
        TARGETWORKSPACE->m_fullscreenMode = request.effectiveMode;
    }
    else
        return FULLSCREEN_REQUEST_FAILED;


        

    // TODO: do this in the unified recalculate. At that point, the workspace's vars should have been properly set by scrolling's recalculate() as well
    // g_pDesktopAnimationManager->setFullscreenFadeAnimation(request.effectiveMode == FSMODE_NONE? CDesktopAnimationManager::ANIMATION_TYPE_OUT : CDesktopAnimationManager::ANIMATION_TYPE_IN);
    


    return FULLSCREEN_REQUEST_DEFAULT;
}

SP<ITarget> IModeAlgorithm::layoutFullscreenTarget() const {
    return nullptr;
}

// REDUNDANT: all FS windows must set their internal FS state
// bool IModeAlgorithm::layoutFullscreenCoversMonitor() const {
//     return false;
// }

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
