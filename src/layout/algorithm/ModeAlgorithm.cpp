#include "ModeAlgorithm.hpp"

#include "../space/Space.hpp"
#include "Algorithm.hpp"
#include "../../output/Monitor.hpp"
#include "../../desktop/view/Window.hpp"
#include "Compositor.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/Workspace.hpp"
#include "desktop/view/LayerSurface.hpp"
#include "layout/LayoutManager.hpp"
#include "managers/animation/DesktopAnimationManager.hpp"
#include "managers/fullscreen/handler/FullscreenHandler.hpp" // or wherever it ends up

using namespace Layout;

IModeAlgorithm::IModeAlgorithm() : m_defaultFullscreenHandler(makeUnique<Fullscreen::IFullscreenHandler>(this)) {}

Config::ErrorResult IModeAlgorithm::layoutMsg(const std::string_view& sv) {
    return {};
}

std::optional<Vector2D> IModeAlgorithm::predictSizeForNewTarget() {
    return std::nullopt;
}

WP<Fullscreen::IFullscreenHandler> IModeAlgorithm::getFSHandler() {

    return m_defaultFullscreenHandler;

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
