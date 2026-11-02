#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
uniform sampler2D sharpTex;

uniform int acrylicEnabled;
uniform vec4 acrylicExtent;
uniform vec4      acrylicRadius;
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
#include "rounding.glsl"

layout(location = 0) out vec4 fragColor;

float acrylicLength(vec2 value, float power) {
    value = abs(value);
    float scale = max(value.x, value.y);
    if (scale == 0.0)
        return 0.0;

    value /= scale;
    return scale * pow(pow(value.x, power) + pow(value.y, power), 1.0 / power);
}

float roundedBoxSDF(vec2 position, vec2 halfSize, float radius, float power) {
    radius = clamp(radius, 0.0, min(halfSize.x, halfSize.y));
    vec2 offset = abs(position) - (halfSize - vec2(radius));
    vec2 outside = max(offset, vec2(0.0));
    float cornerDistance = acrylicLength(outside, power);
    return cornerDistance + min(max(offset.x, offset.y), 0.0) - radius;
}

float smootherstep(float edge0, float edge1, float value) {
    float progress = clamp((value - edge0) / max(edge1 - edge0, 0.0001), 0.0, 1.0);
    return progress * progress * progress * (progress * (progress * 6.0 - 15.0) + 10.0);
}

vec3 acrylicLens(vec2 position, vec2 halfSize, float radius, float power, float width) {
    // Extend the optical corners across the entire bulb, independently of the clipping radius.
    // Its normal can only become undefined in the flat core, where refraction is already zero.
    float opticalRadius = max(radius, width);
    vec2 corner = max(abs(position) - (halfSize - vec2(opticalRadius)), vec2(0.0));
    float curvature = smootherstep(opticalRadius - width, opticalRadius, acrylicLength(corner, power));

    // Soften the normal transitions for angular outlines without changing their optical depth.
    corner /= max(max(corner.x, corner.y), 0.0001);
    vec2 gradient = pow(corner, vec2(max(power - 1.0, 1.0))) * sign(position);
    vec2 outward = gradient / max(length(gradient), 0.0001);
    return vec3(outward, curvature);
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
    if (minimumHalfSize <= 0.0001)
        return blurred;

    float boundaryRadius = clamp(cornerRadius(acrylicRadius, position), 0.0, minimumHalfSize);
    float bulbWidth = min(max(acrylicBulb, 1.0), minimumHalfSize * 0.8);

    float boundarySdf = roundedBoxSDF(position, halfSize, boundaryRadius, power);
    float antialias = max(fwidth(boundarySdf), 0.75);
    float shape = 1.0 - smoothstep(-antialias, antialias, boundarySdf);
    vec2 texcoordDx = dFdx(v_texcoord);
    vec2 texcoordDy = dFdy(v_texcoord);
    if (shape <= 0.0)
        return blurred;

    vec3 lens = acrylicLens(position, halfSize, boundaryRadius, power, bulbWidth);
    vec2 outward = lens.xy;
    float curvature = lens.z;
    vec2 sourceSize = vec2(textureSize(tex, 0));
    vec2 halfTexel = 0.5 / sourceSize;
    vec2 maximumUV = vec2(1.0) - halfTexel;
    vec2 inwardUV = -(outward.x * texcoordDx + outward.y * texcoordDy);
    float maximumRefraction = max(acrylicRefraction, 0.0);
    float availableTravel = availableTextureTravel(v_texcoord, inwardUV, halfTexel, maximumUV);
    float edgeValidity = smoothstep(0.0, maximumRefraction + 1.0, availableTravel);
    float safeRefraction = min(maximumRefraction * edgeValidity, max(availableTravel - 0.5, 0.0));
    vec2 displacementUV = inwardUV * safeRefraction * curvature * effect;
    float aberration = clamp(acrylicAberration, 0.0, 0.25);
    vec2 redUV = clamp(v_texcoord + displacementUV, halfTexel, maximumUV);
    vec2 greenUV = clamp(v_texcoord + displacementUV * (1.0 - aberration * 0.5), halfTexel, maximumUV);
    vec2 blueUV = clamp(v_texcoord + displacementUV * (1.0 - aberration), halfTexel, maximumUV);

    float outputAlpha = blurred.a;
    // Refract both backdrops before mixing, so frosting does not wash out the lens.
    vec4 refractedBlurred = blurred;
    if (curvature > 0.0001 && safeRefraction > 0.0001)
        refractedBlurred = texture(tex, greenUV);
    vec3 acrylicLinear = toLinearRGB(refractedBlurred.rgb / max(refractedBlurred.a, 0.001), acrylicTransferFunction);

    float fresnelTransmission = mix(1.0, 0.84, curvature);
    float clarity = clamp(acrylicClarity, 0.0, 1.0) * effect * fresnelTransmission;
    if (clarity > 0.0001) {
        vec4 refractedGreen = texture(sharpTex, greenUV);
        vec3 refractedLinear = toLinearRGB(refractedGreen.rgb / max(refractedGreen.a, 0.001), acrylicTransferFunction);
        if (aberration > 0.0001 && curvature > 0.0001 && safeRefraction > 0.0001) {
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

    const vec2 LIGHT_DIRECTION = vec2(-0.451219, 0.892413);
    float directional = dot(outward, LIGHT_DIRECTION);
    float rim = 1.0 - smootherstep(0.0, max(bulbWidth * 0.08, 1.0), max(-boundarySdf, 0.0));
    float backdropLuma = dot(acrylicLinear, vec3(0.2126, 0.7152, 0.0722));
    float normalizedLuma = clamp(backdropLuma / max(acrylicLuminanceScale, 0.001), 0.0, 1.0);
    float highlight = rim * (0.025 + 0.075 * max(directional, 0.0));
    highlight *= effect * mix(1.0, 0.4, normalizedLuma);

    float shadow = curvature * 0.025 * max(-directional, 0.0);
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
