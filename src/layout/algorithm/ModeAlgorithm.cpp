#include "ModeAlgorithm.hpp"

#include "../space/Space.hpp"
#include "Algorithm.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../desktop/view/Window.hpp"

using namespace Layout;

std::expected<void, std::string> IModeAlgorithm::layoutMsg(const std::string_view& sv) {
    return {};
}

std::optional<Vector2D> IModeAlgorithm::predictSizeForNewTarget() {
    return std::nullopt;
}

std::optional<Vector2D> IModeAlgorithm::focalPointForDir(SP<ITarget> t, Math::eDirection dir) {
    Vector2D   focalPoint;

    const auto WINDOWIDEALBB =
        t->fullscreenMode() != FSMODE_NONE ? m_parent->space()->workspace()->m_monitor->logicalBox() : t->window()->getWindowIdealBoundingBoxIgnoreReserved();

    switch (dir) {
        case Math::DIRECTION_UP: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x / 2.0, -1.0}; break;
        case Math::DIRECTION_DOWN: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x / 2.0, WINDOWIDEALBB.size().y + 1.0}; break;
        case Math::DIRECTION_LEFT: focalPoint = WINDOWIDEALBB.pos() + Vector2D{-1.0, WINDOWIDEALBB.size().y / 2.0}; break;
        case Math::DIRECTION_RIGHT: focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x + 1.0, WINDOWIDEALBB.size().y / 2.0}; break;
        default: return std::nullopt;
    }

    return focalPoint;
}
