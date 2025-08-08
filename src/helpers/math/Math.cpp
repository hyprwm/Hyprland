#include "Math.hpp"
#include "../memory/Memory.hpp"

Hyprutils::Math::eTransform wlTransformToHyprutils(wl_output_transform t) {
    switch (t) {
        case WL_OUTPUT_TRANSFORM_NORMAL: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_NORMAL;
        case WL_OUTPUT_TRANSFORM_180: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_180;
        case WL_OUTPUT_TRANSFORM_90: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_90;
        case WL_OUTPUT_TRANSFORM_270: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_270;
        case WL_OUTPUT_TRANSFORM_FLIPPED: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_270;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_90;
        default: break;
    }
    return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_NORMAL;
}

wl_output_transform invertTransform(wl_output_transform tr) {
    if ((tr & WL_OUTPUT_TRANSFORM_90) && !(tr & WL_OUTPUT_TRANSFORM_FLIPPED))
        tr = sc<wl_output_transform>(tr ^ sc<int>(WL_OUTPUT_TRANSFORM_180));

    return tr;
}
