#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif

#include "defines.h"

#if USE_CM
#include "cm_helpers.glsl"
#endif

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 1689.1984);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec4 blurFinish(vec4 pixColor, vec2 v_texcoord, float noise, float brightness
#if USE_CM
                ,
                int sourceTF, int targetTF, mat3 convertMatrix, vec2 srcTFRange, vec2 dstTFRange
#endif
) {
    // noise
    float noiseHash   = hash(v_texcoord);
    float noiseAmount = noiseHash - 0.5;
    pixColor.rgb += noiseAmount * noise;

    // brightness
    pixColor.rgb *= min(1.0, brightness);

#if USE_CM
    pixColor = doColorManagement(pixColor, sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange);
#endif

    return pixColor;
}
