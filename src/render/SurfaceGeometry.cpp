#include "SurfaceGeometry.hpp"
#include "../desktop/view/window/Window.hpp"

#include <cmath>

Vector2D Render::subsurfaceScale(const Vector2D& windowSize, const Vector2D& reportedSize, bool animating) {
    const auto axis = [animating](double size, double reported) {
        if (reported <= 0 || size <= 0 || !std::isfinite(size) || !std::isfinite(reported))
            return 1.0;

        // Wayland configures are integral. Preserve the fractional remainder of a
        // settled allocation, without stretching clients that chose another size.
        if (!animating && std::floor(size) != reported)
            return 1.0;

        return size / reported;
    };
    return {axis(windowSize.x, reportedSize.x), axis(windowSize.y, reportedSize.y)};
}

Vector2D Render::windowSubsurfaceScale(PHLWINDOW window) {
    if (!window)
        return {1, 1};

    const bool ANIMATING = window->sizeAnimation()->isBeingAnimated();
    if (!ANIMATING && window->backend().isX11())
        return {1, 1};

    return subsurfaceScale(window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT), window->backend().reportedSize(), ANIMATING);
}
