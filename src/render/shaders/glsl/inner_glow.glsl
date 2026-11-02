#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif
#ifndef INNER_GLOW_GLSL
#define INNER_GLOW_GLSL

#include "cm_helpers.glsl"
#include "gradient.glsl"
#include "rounding.glsl"

float innerGlowAlpha(float distFromEdge, float range, float glowPower) {
    if (distFromEdge >= range)
        return 0.0;

    if (distFromEdge <= 0.0)
        return 1.0;

    return pow(1.0 - distFromEdge / range, glowPower);
}

float innerGlowSmin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

vec4 getInnerGlow(vec4 pixColor, vec4 colorSRGB, vec2 v_texcoord, vec4 radii, float roundingPower, vec2 topLeft, vec2 fullSize, float range, float glowPower, vec2 bottomRight,
                  int gradientLength, vec4 gradient[10], float angle, int gradient2Length, vec4 gradient2[10], float angle2, float gradientLerp, float alpha) {
    vec2 pixCoord = fullSize * v_texcoord;

    if (gradientLength > 0 || gradient2Length > 0)
        pixColor = getColorForCoord(v_texcoord, gradientLength, gradient, angle, gradient2Length, gradient2, angle2, gradientLerp);
    pixColor.a *= alpha;

    // Match the border's inner corner contour and antialiasing.
    vec2  cornerCoord = pixCoord - fullSize * 0.5;
    float radius      = cornerRadius(radii, cornerCoord);
    cornerCoord       = abs(cornerCoord) - (fullSize * 0.5 - radius);
    cornerCoord      += vec2(1.0) / fullSize;

    if (radius > 0.0 && min(cornerCoord.x, cornerCoord.y) > 0.0) {
        float dist = distanceWithRounding(cornerCoord, roundingPower);
        if (dist > radius + SMOOTHING_CONSTANT)
            discard;

        pixColor.a *= 1.0 - smoothstep(radius - SMOOTHING_CONSTANT, radius + SMOOTHING_CONSTANT, dist);
    }

    // smooth-min of edge distances for rounded glow contours
    float distT = pixCoord.y;
    float distB = fullSize.y - pixCoord.y;
    float distL = pixCoord.x;
    float distR = fullSize.x - pixCoord.x;

    float k = range;
    float distFromEdge = innerGlowSmin(innerGlowSmin(distT, distB, k), innerGlowSmin(distL, distR, k), k);

    pixColor[3] = pixColor[3] * innerGlowAlpha(distFromEdge, range, glowPower);

    if (pixColor[3] == 0.0) 
        discard;

    // premultiply
    pixColor.rgb *= pixColor[3];

#if USE_MIRROR
    vec4[2] pixColors;
    pixColors[0] = pixColor;
    pixColors[1] = colorSRGB;
    pixColors[1].a = pixColor.a;
    pixColors[1].rgb *= pixColors[1].a;
    return pixColors;
#else
    return pixColor;
#endif
}
#endif
