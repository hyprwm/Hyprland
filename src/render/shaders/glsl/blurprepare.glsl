#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif

#include "defines.h"

#if USE_CM
#include "cm_helpers.glsl"
#endif

#include "gain.glsl"

vec4 blurPrepare(vec4 pixColor, float contrast, float brightness
#if USE_CM
                 ,
                 int sourceTF, int targetTF, mat3 convertMatrix, vec2 srcTFRange, vec2 dstTFRange, float srcRefLuminance, float sdrBrightnessMultiplier
#endif
) {
#if USE_CM
    if (sourceTF == CM_TRANSFER_FUNCTION_ST2084_PQ) {
        pixColor.rgb /= sdrBrightnessMultiplier;
    }
    pixColor.rgb = convertMatrix * toLinearRGB(pixColor.rgb, sourceTF);
    pixColor     = toNit(pixColor, vec2(srcTFRange[0], srcRefLuminance));
    pixColor     = fromLinearNit(pixColor, targetTF, dstTFRange);
#endif

    // contrast
    if (contrast != 1.0)
        pixColor.rgb = gain(pixColor.rgb, contrast);

    // brightness
    pixColor.rgb *= max(1.0, brightness);

    return pixColor;
}
