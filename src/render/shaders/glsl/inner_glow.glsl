#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif
#ifndef INNER_GLOW_GLSL
#define INNER_GLOW_GLSL

float innerGlowAlpha(float distFromEdge, float range, float glowPower) {
    if (distFromEdge >= range)
        return 0.0;

    if (distFromEdge <= 0.0)
        return 1.0;

    return pow(1.0 - distFromEdge / range, glowPower);
}

float innerGlowModifiedLength(vec2 a, float roundingPower) {
    return pow(pow(abs(a.x), roundingPower) + pow(abs(a.y), roundingPower), 1.0 / roundingPower);
}

float innerGlowSmin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

vec4 getInnerGlow(vec4 pixColor, vec4 colorSRGB, vec2 v_texcoord, float radius, float roundingPower, vec2 topLeft, vec2 fullSize, float range, float glowPower, vec2 bottomRight) {
    vec2 pixCoord = fullSize * v_texcoord;

    // clip to the rounded rectangle shape using actual SDF
    vec2  center  = fullSize * 0.5;
    vec2  p       = abs(pixCoord - center);
    vec2  q       = p - (center - vec2(radius));
    vec2  qc      = max(q, vec2(0.0));
    float cornerD = (qc.x > 0.0 || qc.y > 0.0) ? innerGlowModifiedLength(qc, roundingPower) : 0.0;
    float sdfDist = cornerD + min(max(q.x, q.y), 0.0) - radius;

    if (sdfDist > 0.0)
        discard;

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
