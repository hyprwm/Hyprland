#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif

#include "defines.h"

#include "gain.glsl"

vec4 blurPrepare(vec4 pixColor, float contrast, float brightness) {
    // contrast
    if (contrast != 1.0)
        pixColor.rgb = gain(pixColor.rgb, contrast);

    // brightness
    pixColor.rgb *= max(1.0, brightness);

    return pixColor;
}
