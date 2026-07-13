#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif
#include "cm_helpers.glsl"
#include "gradient.glsl"
#if USE_ROUNDING
#include "rounding.glsl"
#endif

#if USE_MIRROR
vec4[2]
#else
vec4
#endif
    getBorder(vec2 v_texcoord, float alpha, vec2 fullSizeUntransformed, float radiusOuter, float thick, float radius, float roundingPower, vec2 topLeft, vec2 fullSize,
              int gradientLength, vec4 gradient[10], float angle, int gradient2Length, vec4 gradient2[10], float angle2, float gradientLerp
#if USE_CM
              ,
              int sourceTF, int targetTF, mat3 convertMatrix, vec2 srcTFRange, vec2 dstTFRange
#if USE_ICC
              ,
              highp sampler3D iccLut3D, float iccLutSize
#else
#if USE_TONEMAP || USE_SDR_MOD
              ,
              mat3 targetPrimariesXYZ
#endif
#if USE_TONEMAP
              ,
              float maxLuminance, float dstMaxLuminance, float dstRefLuminance, float srcRefLuminance, int tonemapMode
#endif
#if USE_SDR_MOD
              ,
              float sdrSaturation, float sdrBrightnessMultiplier
#endif
#endif
#endif
    ) {
    vec2 pixCoord         = vec2(gl_FragCoord);
    vec2 pixCoordOuter    = pixCoord;
    vec2 originalPixCoord = v_texcoord;
    originalPixCoord *= fullSizeUntransformed;
    float additionalAlpha = 1.0;

    vec4  pixColor = vec4(1.0, 1.0, 1.0, 1.0);

    bool  done = false;

    pixCoord -= topLeft + fullSize * 0.5;
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoordOuter = pixCoord;
    pixCoord -= fullSize * 0.5 - radius;
    pixCoordOuter -= fullSize * 0.5 - radiusOuter;

    // center the pixels don't make it top-left
    pixCoord += vec2(1.0, 1.0) / fullSize;
    pixCoordOuter += vec2(1.0, 1.0) / fullSize;

#if USE_ROUNDING
    if (min(pixCoord.x, pixCoord.y) > 0.0 && radius > 0.0) {
        float dist      = pow(pow(pixCoord.x, roundingPower) + pow(pixCoord.y, roundingPower), 1.0 / roundingPower);
        float distOuter = pow(pow(pixCoordOuter.x, roundingPower) + pow(pixCoordOuter.y, roundingPower), 1.0 / roundingPower);
        float h         = (thick / 2.0);

        if (dist < radius - h) {
            // lower
            float normalized = smoothstep(0.0, 1.0, (dist - radius + thick + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));
            additionalAlpha *= normalized;
            done = true;
        } else if (min(pixCoordOuter.x, pixCoordOuter.y) > 0.0) {
            // higher
            float normalized = 1.0 - smoothstep(0.0, 1.0, (distOuter - radiusOuter + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));
            additionalAlpha *= normalized;
            done = true;
        } else if (distOuter < radiusOuter - h) {
            additionalAlpha = 1.0;
            done            = true;
        }
    }
#endif

    // now check for other shit
    if (!done) {
        // distance to all straight bb borders
        float distanceT = originalPixCoord[1];
        float distanceB = fullSizeUntransformed[1] - originalPixCoord[1];
        float distanceL = originalPixCoord[0];
        float distanceR = fullSizeUntransformed[0] - originalPixCoord[0];

        // get the smallest
        float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));

        if (smallest > thick)
            discard;
    }

    if (additionalAlpha == 0.0)
        discard;

    pixColor = getColorForCoord(v_texcoord, gradientLength, gradient, angle, gradient2Length, gradient2, angle2, gradientLerp);
    pixColor.rgb *= pixColor[3];

#if USE_CM
    return doColorManagement(pixColor, alpha * additionalAlpha, sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#if USE_ICC
                             ,
                             iccLut3D, iccLutSize
#else
#if USE_TONEMAP || USE_SDR_MOD
                             ,
                             targetPrimariesXYZ
#endif
#if USE_TONEMAP
                             ,
                             maxLuminance, dstMaxLuminance, dstRefLuminance, srcRefLuminance, tonemapMode
#endif
#if USE_SDR_MOD
                             ,
                             sdrSaturation, sdrBrightnessMultiplier
#endif
#endif
    );
#endif

#if USE_MIRROR
    vec4[2] pixColors;
    pixColors[0] = pixColor * alpha * additionalAlpha;
    pixColors[1] = pixColors[0];
    return pixColors;
#else
    pixColor *= alpha * additionalAlpha;
    return pixColor;
#endif
}
