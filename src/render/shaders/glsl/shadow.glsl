#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif
#ifndef SHADOW_GLSL
#define SHADOW_GLSL

#include "cm_helpers.glsl"
#include "rounding.glsl"

float pixAlphaRoundedDistance(float distanceToCorner, float radius, float range, float shadowPower) {
    if (distanceToCorner > radius) {
        return 0.0;
    }

    if (distanceToCorner > radius - range) {
        return pow((range - (distanceToCorner - radius + range)) / range, shadowPower); // i think?
    }

    return 1.0;
}

float modifiedLength(vec2 a, float roundingPower) {
    return pow(pow(abs(a.x), roundingPower) + pow(abs(a.y), roundingPower), 1.0 / roundingPower);
}

#if USE_MIRROR
vec4[2]
#else
vec4
#endif
    getShadow(vec4 pixColor, vec2 v_texcoord, float borderRadius, float roundingPower, vec2 topLeft, vec2 fullSize, float range, float shadowPower, vec2 bottomRight,
              float decoWidth
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
              float maxLuminance, float dstMaxLuminance, float dstRefLuminance, float srcRefLuminance
#endif
#if USE_SDR_MOD
              ,
              float sdrSaturation, float sdrBrightnessMultiplier
#endif
#endif
#endif
    ) {
    float radius        = range + borderRadius;
    float originalAlpha = pixColor[3];

    bool  done = false;

    vec2  pixCoord = fullSize * v_texcoord;

    // ok, now we check the distance to a border.
    // corners
    if (pixCoord[0] < topLeft[0]) {
        if (pixCoord[1] < topLeft[1]) {
            // top left
            float distance = distanceWithRounding(vec2(topLeft.x - pixCoord.x, topLeft.y - pixCoord.y), roundingPower);
            if (borderRadius > 0.0 && distance < decoWidth) {
                pixColor[3] = 0.0;
            } else {
                pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - topLeft, roundingPower), radius, range, shadowPower);
            }
            done = true;
        } else if (pixCoord[1] > bottomRight[1]) {
            // bottom left
            float distance = distanceWithRounding(vec2(topLeft.x - pixCoord.x, pixCoord.y - bottomRight.y), roundingPower);
            if (borderRadius > 0.0 && distance < decoWidth) {
                pixColor[3] = 0.0;
            } else {
                pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - vec2(topLeft[0], bottomRight[1]), roundingPower), radius, range, shadowPower);
            }
            done = true;
        }
    } else if (pixCoord[0] > bottomRight[0]) {
        if (pixCoord[1] < topLeft[1]) {
            // top right
            float distance = distanceWithRounding(vec2(pixCoord.x - bottomRight.x, topLeft.y - pixCoord.y), roundingPower);
            if (borderRadius > 0.0 && distance < decoWidth) {
                pixColor[3] = 0.0;
            } else {
                pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - vec2(bottomRight[0], topLeft[1]), roundingPower), radius, range, shadowPower);
            }
            done = true;
        } else if (pixCoord[1] > bottomRight[1]) {
            // bottom right
            float distance = distanceWithRounding(vec2(pixCoord.x - bottomRight.x, pixCoord.y - bottomRight.y), roundingPower);
            if (borderRadius > 0.0 && distance < decoWidth) {
                pixColor[3] = 0.0;
            } else {
                pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - bottomRight, roundingPower), radius, range, shadowPower);
            }
            done = true;
        }
    }

    // edges
    if (!done) {
        // distance to all straight bb borders
        float distanceT = pixCoord[1];
        float distanceB = fullSize[1] - pixCoord[1];
        float distanceL = pixCoord[0];
        float distanceR = fullSize[0] - pixCoord[0];

        // get the smallest
        float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));

        if (smallest < range) {
            // between border and max shadow distance
            pixColor[3] = pixColor[3] * pow((smallest / range), shadowPower);
        } else {
            // inside border or window
            pixColor[3] = 0.0;
        }
    }

    if (pixColor[3] == 0.0) {
        discard;
#if USE_MIRROR
        vec4[2] pixColors;
        pixColors[0] = pixColor;
        pixColors[1] = pixColor;
        return pixColors;
#else
        return pixColor;
#endif
    }

    // premultiply
    pixColor.rgb *= pixColor[3];

#if USE_CM
#if USE_MIRROR
    vec4[2] pixColors =
#else
    pixColor =
#endif
        doColorManagement(pixColor, sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
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
                          maxLuminance, dstMaxLuminance, dstRefLuminance, srcRefLuminance
#endif
#if USE_SDR_MOD
                          ,
                          sdrSaturation, sdrBrightnessMultiplier
#endif
#endif
        );
#endif
#if USE_MIRROR
    return pixColors;
#else
    return pixColor;
#endif
}
#endif