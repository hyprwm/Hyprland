#include "BorderGeometry.hpp"
#include "../../helpers/math/Math.hpp"

#include <hyprutils/memory/Casts.hpp>

using namespace Hyprutils::Memory;

int Render::borderSizeInPixels(int borderSize, float scale) {
    return sc<int>(Math::scaledPixelLength(borderSize, scale));
}

double Render::borderExtentInLogical(int borderSize, float scale) {
    return borderSizeInPixels(borderSize, scale) / sc<double>(scale);
}
