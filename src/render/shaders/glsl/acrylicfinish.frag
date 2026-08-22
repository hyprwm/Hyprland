#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
uniform sampler2D sharpTex;

uniform int acrylicEnabled;
uniform vec4 acrylicExtent;
uniform float acrylicRadius;
uniform float acrylicRoundingPower;
uniform float acrylicRefraction;
uniform float acrylicBulb;
uniform float acrylicClarity;
uniform float acrylicAberration;
uniform vec4 acrylicTint;
uniform float acrylicStrength;
uniform int acrylicTransferFunction;
uniform float acrylicLuminanceScale;
uniform float noise;
uniform float brightness;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "cm_helpers.glsl"
#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;

float acrylicLength(vec2 value, float power) {
    value = abs(value);
    return pow(pow(value.x, power) + pow(value.y, power), 1.0 / power);
}

float roundedBoxSDF(vec2 position, vec2 halfSize, float radius, float power) {
    radius = clamp(radius, 0.0, min(halfSize.x, halfSize.y));
    vec2 offset = abs(position) - (halfSize - vec2(radius));
    vec2 outside = max(offset, vec2(0.0));
    float cornerDistance = (outside.x > 0.0 || outside.y > 0.0) ? acrylicLength(outside, power) : 0.0;
    return cornerDistance + min(max(offset.x, offset.y), 0.0) - radius;
}

float smootherstep(float edge0, float edge1, float value) {
    float progress = clamp((value - edge0) / max(edge1 - edge0, 0.0001), 0.0, 1.0);
    return progress * progress * progress * (progress * (progress * 6.0 - 15.0) + 10.0);
}

float nestedRoundedBoxSDF(vec2 position, vec2 halfSize, float radius, float power, float inset) {
    vec2 nestedHalfSize = max(halfSize - vec2(inset), vec2(0.001));
    float nestedRadius = clamp(radius, 0.0, min(nestedHalfSize.x, nestedHalfSize.y));
    return roundedBoxSDF(position, nestedHalfSize, nestedRadius, power);
}

float roundedProfileDepth(vec2 position, vec2 halfSize, float radius, float power, float width, float boundarySdf) {
    if (boundarySdf >= 0.0)
        return 0.0;

    if (nestedRoundedBoxSDF(position, halfSize, radius, power, width) <= 0.0)
        return width;

    float lower = 0.0;
    float upper = width;
    for (int i = 0; i < 8; ++i) {
        float middle = (lower + upper) * 0.5;
        if (nestedRoundedBoxSDF(position, halfSize, radius, power, middle) <= 0.0)
            lower = middle;
        else
            upper = middle;
    }

    return (lower + upper) * 0.5;
}

vec2 roundedProfileNormal(vec2 position, vec2 halfSize, float radius, float power, float inset) {
    vec2 nestedHalfSize = max(halfSize - vec2(inset), vec2(0.001));
    float nestedRadius = clamp(radius, 0.0, min(nestedHalfSize.x, nestedHalfSize.y));
    vec2 corner = max(abs(position) - (nestedHalfSize - vec2(nestedRadius)), vec2(0.0));
    vec2 gradient;

    if (corner.x > 0.0001 || corner.y > 0.0001) {
        if (power <= 1.0001)
            gradient = vec2(corner.x > 0.0001 ? 1.0 : 0.0, corner.y > 0.0001 ? 1.0 : 0.0);
        else
            gradient = pow(corner, vec2(power - 1.0));
    } else {
        vec2 edgeDistance = nestedHalfSize - abs(position);
        gradient = edgeDistance.x < edgeDistance.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    }

    gradient *= sign(position);
    return gradient / max(length(gradient), 0.0001);
}

float availableTextureTravel(vec2 origin, vec2 direction, vec2 minimumUV, vec2 maximumUV) {
    float travel = 1e10;

    if (direction.x > 0.000001)
        travel = min(travel, (maximumUV.x - origin.x) / direction.x);
    else if (direction.x < -0.000001)
        travel = min(travel, (minimumUV.x - origin.x) / direction.x);

    if (direction.y > 0.000001)
        travel = min(travel, (maximumUV.y - origin.y) / direction.y);
    else if (direction.y < -0.000001)
        travel = min(travel, (minimumUV.y - origin.y) / direction.y);

    return max(travel, 0.0);
}

vec4 applyAcrylic(vec4 blurred) {
    float effect = clamp(acrylicStrength, 0.0, 1.0);
    if (effect <= 0.0)
        return blurred;

    vec2 halfSize = acrylicExtent.zw * 0.5;
    vec2 center = acrylicExtent.xy + halfSize;
    vec2 position = gl_FragCoord.xy - center;
    float power = clamp(acrylicRoundingPower, 1.0, 10.0);
    float minimumHalfSize = min(halfSize.x, halfSize.y);
    float boundaryRadius = clamp(acrylicRadius, 0.0, minimumHalfSize);
    float maximumBulb = max(minimumHalfSize * 0.8, 1.0);
    float bulbWidth = clamp(acrylicBulb, 1.0, maximumBulb);

    float boundarySdf = roundedBoxSDF(position, halfSize, boundaryRadius, power);
    float antialias = max(fwidth(boundarySdf), 0.75);
    float shape = 1.0 - smoothstep(-antialias, antialias, boundarySdf);
    vec2 texcoordDx = dFdx(v_texcoord);
    vec2 texcoordDy = dFdy(v_texcoord);
    if (shape <= 0.0)
        return blurred;

    float opticalDepth = roundedProfileDepth(position, halfSize, boundaryRadius, power, bulbWidth, boundarySdf);
    float progress = clamp(opticalDepth / bulbWidth, 0.0, 1.0);
    float clarityCore = smootherstep(0.25, 0.6, progress);
    float curvature = 1.0 - smootherstep(0.0, 1.0, progress);
    float lensEntry = smootherstep(0.0, 0.08, progress);
    float lensExit = 1.0 - smootherstep(0.16, 1.0, progress);
    float lens = lensEntry * lensExit;
    float outerRim = 1.0 - smootherstep(0.0, max(bulbWidth * 0.07, 2.0), opticalDepth);
    float caustic = smootherstep(0.04, 0.13, progress) * (1.0 - smootherstep(0.22, 0.48, progress));
    float counterRim = smootherstep(0.18, 0.34, progress) * (1.0 - smootherstep(0.42, 0.72, progress));

    vec2 outward = roundedProfileNormal(position, halfSize, boundaryRadius, power, opticalDepth);
    vec2 sourceSize = vec2(textureSize(tex, 0));
    vec2 halfTexel = 0.5 / sourceSize;
    vec2 maximumUV = vec2(1.0) - halfTexel;
    vec2 outwardUV = outward.x * texcoordDx + outward.y * texcoordDy;
    float maximumRefraction = max(acrylicRefraction, 0.0);
    float availableTravel = availableTextureTravel(v_texcoord, outwardUV, halfTexel, maximumUV);
    float edgeValidity = smoothstep(0.0, maximumRefraction + 1.0, availableTravel);
    float safeRefraction = min(maximumRefraction * edgeValidity, max(availableTravel - 0.5, 0.0));
    vec2 displacementPixels = outward * safeRefraction * lens * effect;
    vec2 displacementUV = displacementPixels.x * texcoordDx + displacementPixels.y * texcoordDy;
    float aberration = clamp(acrylicAberration, 0.0, 0.25);
    vec2 redUV = clamp(v_texcoord + displacementUV, halfTexel, maximumUV);
    vec2 greenUV = clamp(v_texcoord + displacementUV * (1.0 - aberration * 0.5), halfTexel, maximumUV);
    vec2 blueUV = clamp(v_texcoord + displacementUV * (1.0 - aberration), halfTexel, maximumUV);

    float outputAlpha = blurred.a;
    vec3 blurredLinear = toLinearRGB(blurred.rgb / max(blurred.a, 0.001), acrylicTransferFunction);
    vec3 acrylicLinear = blurredLinear;
    if (lens > 0.0001 && safeRefraction > 0.0001) {
        vec4 displacedBlurred = texture(tex, greenUV);
        vec3 displacedBlurredLinear = toLinearRGB(displacedBlurred.rgb / max(displacedBlurred.a, 0.001), acrylicTransferFunction);
        acrylicLinear = mix(acrylicLinear, displacedBlurredLinear, effect * curvature * edgeValidity);
    }

    float fresnelTransmission = mix(1.0, 0.84, curvature);
    float clarity = clamp(acrylicClarity, 0.0, 1.0) * effect * fresnelTransmission * clarityCore;
    if (clarity > 0.0001) {
        vec4 refractedGreen = texture(sharpTex, greenUV);
        vec3 refractedLinear = toLinearRGB(refractedGreen.rgb / max(refractedGreen.a, 0.001), acrylicTransferFunction);
        if (aberration > 0.0001 && lens > 0.0001 && safeRefraction > 0.0001) {
            vec4 refractedRed = texture(sharpTex, redUV);
            vec4 refractedBlue = texture(sharpTex, blueUV);
            vec3 refractedRedLinear = toLinearRGB(refractedRed.rgb / max(refractedRed.a, 0.001), acrylicTransferFunction);
            vec3 refractedBlueLinear = toLinearRGB(refractedBlue.rgb / max(refractedBlue.a, 0.001), acrylicTransferFunction);
            refractedLinear.r = refractedRedLinear.r;
            refractedLinear.b = refractedBlueLinear.b;
        }
        acrylicLinear = mix(acrylicLinear, refractedLinear, clarity);
    }

    vec3 tintLinear = acrylicTint.rgb;
    float thickness = mix(0.34, 1.0, curvature);
    float transmission = exp(-acrylicTint.a * effect * thickness);
    acrylicLinear = acrylicLinear * transmission + tintLinear * (1.0 - transmission);

    vec3 surfaceNormal = normalize(vec3(outward * curvature * 2.15, 1.0));
    const vec2 LIGHT_DIRECTION = vec2(-0.451219, 0.892413);
    vec3 light = normalize(vec3(LIGHT_DIRECTION, 0.72));
    vec3 halfway = normalize(light + vec3(0.0, 0.0, 1.0));
    float oneMinusNV = 1.0 - max(surfaceNormal.z, 0.0);
    float fresnel = 0.0204 + 0.9796 * pow(oneMinusNV, 5.0);
    float specular = pow(max(dot(surfaceNormal, halfway), 0.0), 40.0);
    float directional = dot(outward, LIGHT_DIRECTION);
    float backdropLuma = dot(acrylicLinear, vec3(0.2126, 0.7152, 0.0722));
    float normalizedLuma = clamp(backdropLuma / max(acrylicLuminanceScale, 0.001), 0.0, 1.0);
    float highlight = outerRim * (0.045 + 0.12 * max(directional, 0.0));
    highlight += caustic * (0.035 + 0.08 * max(directional, 0.0));
    highlight += curvature * (0.08 * fresnel + 0.13 * specular);
    highlight *= effect * mix(1.0, 0.4, normalizedLuma);

    float shadow = outerRim * 0.09 * max(-directional, 0.0) + counterRim * (0.025 + 0.05 * max(-directional, 0.0));
    shadow *= effect * mix(0.45, 1.0, normalizedLuma);
    acrylicLinear *= 1.0 - shadow;
    acrylicLinear += mix(vec3(acrylicLuminanceScale), tintLinear, 0.08) * highlight;

    vec4 acrylic = fromLinear(vec4(max(acrylicLinear, vec3(0.0)) * outputAlpha, outputAlpha), acrylicTransferFunction);
    return mix(blurred, acrylic, shape);
}

void main() {
    vec4 color = texture(tex, v_texcoord);
    if (acrylicEnabled != 0)
        color = applyAcrylic(color);

    fragColor = blurFinish(color, v_texcoord, noise, brightness
#if USE_CM
                           ,
                           sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
