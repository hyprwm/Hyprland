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

float roundedBoundaryTravel(vec2 position, vec2 direction, vec2 halfSize, float radius, float power, float boundarySdf) {
    if (boundarySdf >= 0.0)
        return 0.0;

    float horizontalTravel = 1e10;
    if (direction.x > 0.000001)
        horizontalTravel = (halfSize.x - position.x) / direction.x;
    else if (direction.x < -0.000001)
        horizontalTravel = (-halfSize.x - position.x) / direction.x;

    float verticalTravel = 1e10;
    if (direction.y > 0.000001)
        verticalTravel = (halfSize.y - position.y) / direction.y;
    else if (direction.y < -0.000001)
        verticalTravel = (-halfSize.y - position.y) / direction.y;

    float lower = 0.0;
    float upper = max(min(horizontalTravel, verticalTravel), 0.0);
    for (int i = 0; i < 12; ++i) {
        if (upper - lower <= 0.25)
            break;

        float middle = (lower + upper) * 0.5;
        if (roundedBoxSDF(position + direction * middle, halfSize, radius, power) < 0.0)
            lower = middle;
        else
            upper = middle;
    }

    return upper;
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
    float opticalRadius = min(minimumHalfSize, max(boundaryRadius, bulbWidth));
    float opticalSdf = roundedBoxSDF(position, halfSize, opticalRadius, power);
    float antialias = max(fwidth(boundarySdf), 0.75);
    float shape = 1.0 - smoothstep(-antialias, antialias, boundarySdf);
    vec2 opticalGradient = vec2(dFdx(opticalSdf), dFdy(opticalSdf));
    vec2 texcoordDx = dFdx(v_texcoord);
    vec2 texcoordDy = dFdy(v_texcoord);
    if (shape <= 0.0)
        return blurred;

    float opticalDepth = max(-opticalSdf, 0.0);
    float progress = clamp(opticalDepth / bulbWidth, 0.0, 1.0);
    float edgeWeight = 1.0 - progress;
    float lens = pow(1.0 - smoothstep(0.0, 1.0, progress), 0.75);
    float boundaryDepth = max(-boundarySdf, 0.0);
    float rim = 1.0 - smoothstep(0.0, max(bulbWidth * 0.12, 2.0), boundaryDepth);

    vec2 outward = opticalGradient / max(length(opticalGradient), 0.0001);
    vec2 sourceSize = vec2(textureSize(tex, 0));
    vec2 halfTexel = 0.5 / sourceSize;
    vec2 maximumUV = vec2(1.0) - halfTexel;
    vec2 outwardUV = outward.x * texcoordDx + outward.y * texcoordDy;
    float maximumRefraction = acrylicRefraction;
    float edgeValidity = 1.0;
    float safeRefraction = maximumRefraction;
    vec2 sourcePosition = v_texcoord * sourceSize;
    float nearestTextureEdge = min(min(sourcePosition.x, sourceSize.x - sourcePosition.x), min(sourcePosition.y, sourceSize.y - sourcePosition.y));
    if (maximumRefraction > 0.0 && nearestTextureEdge <= maximumRefraction + bulbWidth + 1.0) {
        float boundaryTravel = roundedBoundaryTravel(position, outward, halfSize, boundaryRadius, power, boundarySdf);
        vec2 boundaryUV = v_texcoord + boundaryTravel * outwardUV;
        float availableTravel = availableTextureTravel(boundaryUV, outwardUV, halfTexel, maximumUV);
        edgeValidity = smoothstep(0.0, maximumRefraction + 1.0, availableTravel);
        safeRefraction = min(maximumRefraction * edgeValidity, max(availableTravel - 0.5, 0.0));
    }
    vec2 displacementPixels = outward * safeRefraction * lens;

    float outputAlpha = blurred.a;
    vec3 blurredLinear = toLinearRGB(blurred.rgb / max(blurred.a, 0.001), acrylicTransferFunction);
    float opticalWeight = effect * lens * edgeValidity;
    vec3 acrylicLinear = blurredLinear;
    if (opticalWeight > 0.0001) {
        vec2 displacementUV = displacementPixels.x * texcoordDx + displacementPixels.y * texcoordDy;
        float aberration = clamp(acrylicAberration, 0.0, 0.25);
        vec2 redUV = clamp(v_texcoord + displacementUV, halfTexel, maximumUV);
        vec2 greenUV = clamp(v_texcoord + displacementUV * (1.0 - aberration * 0.5), halfTexel, maximumUV);
        vec2 blueUV = clamp(v_texcoord + displacementUV * (1.0 - aberration), halfTexel, maximumUV);
        vec4 displacedBlurred = texture(tex, greenUV);
        vec4 refractedGreen = texture(sharpTex, greenUV);
        vec3 displacedBlurredLinear = toLinearRGB(displacedBlurred.rgb / max(displacedBlurred.a, 0.001), acrylicTransferFunction);
        vec3 refractedLinear = toLinearRGB(refractedGreen.rgb / max(refractedGreen.a, 0.001), acrylicTransferFunction);
        if (aberration > 0.0001) {
            vec4 refractedRed = texture(sharpTex, redUV);
            vec4 refractedBlue = texture(sharpTex, blueUV);
            vec3 refractedRedLinear = toLinearRGB(refractedRed.rgb / max(refractedRed.a, 0.001), acrylicTransferFunction);
            vec3 refractedBlueLinear = toLinearRGB(refractedBlue.rgb / max(refractedBlue.a, 0.001), acrylicTransferFunction);
            refractedLinear.r = refractedRedLinear.r;
            refractedLinear.b = refractedBlueLinear.b;
        }
        acrylicLinear = mix(acrylicLinear, displacedBlurredLinear, opticalWeight);
        acrylicLinear = mix(acrylicLinear, refractedLinear, acrylicClarity * opticalWeight);
    }

    vec3 tintLinear = toLinearRGB(acrylicTint.rgb, CM_TRANSFER_FUNCTION_SRGB);
    float tintDepth = -log(max(1.0 - acrylicTint.a * effect, 0.0001));
    float thickness = mix(0.28, 1.0, lens);
    float transmission = exp(-tintDepth * thickness);
    acrylicLinear = acrylicLinear * transmission + tintLinear * (1.0 - transmission);

    vec3 surfaceNormal = normalize(vec3(outward * edgeWeight * 1.8, 1.0));
    const vec2 LIGHT_DIRECTION = vec2(-0.451219, 0.892413);
    vec3 light = normalize(vec3(LIGHT_DIRECTION, 0.72));
    vec3 halfway = normalize(light + vec3(0.0, 0.0, 1.0));
    float oneMinusNV = 1.0 - max(surfaceNormal.z, 0.0);
    float fresnel = 0.04 + 0.96 * pow(oneMinusNV, 5.0);
    float specular = pow(max(dot(surfaceNormal, halfway), 0.0), 48.0);
    float directional = dot(outward, LIGHT_DIRECTION);
    float highlight = effect * (rim * 0.14 + lens * (0.12 * fresnel + 0.2 * specular + 0.05 * max(directional, 0.0)));
    acrylicLinear += mix(vec3(1.0), tintLinear, 0.1) * highlight;

    vec4 acrylic = fromLinear(vec4(acrylicLinear * outputAlpha, outputAlpha), acrylicTransferFunction);
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
