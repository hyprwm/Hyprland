#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
uniform sampler2D sharpTex;
uniform sampler2D fluidJarVisualTex;
uniform int fluidJarEnabled;
uniform vec4 fluidJarExtent;
uniform vec4 fluidJarOutputTransform;
uniform vec2 fluidJarOutputOffset;
uniform vec2 fluidJarLogicalSize;
uniform vec4 fluidJarColor;
uniform float fluidJarRefraction;
uniform int fluidJarTransferFunction;
uniform float fluidJarStrength;
uniform float fluidJarTurbulence;
uniform float fluidJarDistortion;
uniform float time;
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

vec4 sampleFluid(vec2 uv) {
    return texture(fluidJarVisualTex, clamp(uv, vec2(0.0), vec2(1.0)));
}

vec2 outputToLogical(vec2 position) {
    return vec2(dot(fluidJarOutputTransform.xy, position), dot(fluidJarOutputTransform.zw, position)) + fluidJarOutputOffset;
}

vec2 logicalToOutputVector(vec2 vector) {
    return vec2(dot(fluidJarOutputTransform.xz, vector), dot(fluidJarOutputTransform.yw, vector));
}

vec2 shimmerWaves(vec2 position, float material) {
    const vec2 DIRECTION_A = vec2(0.894427, 0.447214);
    const vec2 DIRECTION_B = vec2(-0.351123, 0.936329);
    const vec2 DIRECTION_C = vec2(0.196116, -0.980581);
    float materialPhase = 1.4 * material;
    float waveA = cos(0.115 * dot(position, DIRECTION_A) + 0.73 * time + materialPhase);
    float waveB = cos(0.168 * dot(position, DIRECTION_B) - 0.51 * time - 0.8 * materialPhase);
    float waveC = cos(0.237 * dot(position, DIRECTION_C) + 0.37 * time + 1.3 * materialPhase);
    return 0.46 * DIRECTION_A * waveA + 0.34 * DIRECTION_B * waveB + 0.20 * DIRECTION_C * waveC;
}

vec4 applyLiquid(vec4 blurred, vec2 fluidUV, vec2 sourceSize, vec2 sourcePosition, vec2 logicalPosition) {
    vec2 visualSize = vec2(textureSize(fluidJarVisualTex, 0));
    vec2 texel = 1.0 / visualSize;
    vec2 cellPixels = fluidJarLogicalSize / visualSize;

    vec4 center = sampleFluid(fluidUV);
    vec4 left = sampleFluid(fluidUV - vec2(texel.x, 0.0));
    vec4 right = sampleFluid(fluidUV + vec2(texel.x, 0.0));
    vec4 down = sampleFluid(fluidUV - vec2(0.0, texel.y));
    vec4 up = sampleFluid(fluidUV + vec2(0.0, texel.y));

    float field = (4.0 * center.a + left.a + right.a + down.a + up.a) * 0.125;
    float mask = smoothstep(0.18, 0.42, field);
    float opacity = clamp(fluidJarColor.a, 0.0, 1.0);
    float effect = opacity * clamp(fluidJarStrength, 0.0, 1.0);
    if (mask <= 0.0 || effect <= 0.0)
        return blurred;

    vec2 gradient = vec2((right.a - left.a) / max(2.0 * cellPixels.x, 0.001), -(up.a - down.a) / max(2.0 * cellPixels.y, 0.001));
    float gradientLength = length(gradient);
    vec2 outwardNormal = gradientLength > 1e-5 ? -gradient / gradientLength : vec2(0.0);
    float edge = pow(clamp(4.0 * mask * (1.0 - mask), 0.0, 1.0), 0.75) * smoothstep(0.0001, 0.01, gradientLength);

    vec2 centerVelocity = vec2(center.r * cellPixels.x, -center.g * cellPixels.y) * 1.5;
    vec2 leftVelocity = vec2(left.r * cellPixels.x, -left.g * cellPixels.y) * 1.5;
    vec2 rightVelocity = vec2(right.r * cellPixels.x, -right.g * cellPixels.y) * 1.5;
    vec2 downVelocity = vec2(down.r * cellPixels.x, -down.g * cellPixels.y) * 1.5;
    vec2 upVelocity = vec2(up.r * cellPixels.x, -up.g * cellPixels.y) * 1.5;
    vec2 outputVelocity = (2.0 * centerVelocity + leftVelocity + rightVelocity + downVelocity + upVelocity) / 6.0;
    float speed = length(outputVelocity);
    vec2 flowPixels = 2.0 * outputVelocity / (0.8 + speed);

    float motion = smoothstep(0.05, 0.8, speed);
    float turbulence = clamp(fluidJarTurbulence, 0.0, 5.0);
    vec2 turbulentPixels = vec2(0.0);
    if (turbulence > 0.0) {
        float curl = (rightVelocity.y - leftVelocity.y) / max(2.0 * cellPixels.x, 0.001) - (downVelocity.x - upVelocity.x) / max(2.0 * cellPixels.y, 0.001);
        float normalizedCurl = tanh(curl / 0.12);
        vec2 swirlPixels = normalizedCurl * vec2(-outputVelocity.y, outputVelocity.x) / (0.5 + speed);
        vec2 materialGradient = vec2((right.b - left.b) / max(2.0 * cellPixels.x, 0.001), -(up.b - down.b) / max(2.0 * cellPixels.y, 0.001));
        vec2 materialPixels = 4.0 * min(cellPixels.x, cellPixels.y) * materialGradient * mix(0.65, 1.35, motion) * (1.0 + 0.5 * abs(normalizedCurl));
        float shimmerActivity = mix(0.24, 1.25, motion) * (1.0 + 0.35 * abs(normalizedCurl));
        vec2 shimmerPixels = 1.6 * shimmerActivity * shimmerWaves(logicalPosition, center.b);
        turbulentPixels = turbulence * (materialPixels + swirlPixels + shimmerPixels);
    }
    float interior = smoothstep(0.45, 0.8, mask) * (1.0 - edge);
    vec2 interiorPixels = interior * (flowPixels + turbulentPixels);
    interiorPixels /= max(1.0, length(interiorPixels) / max(fluidJarRefraction, 0.001));

    float edgePixels = mix(5.0, fluidJarRefraction, motion);
    float distortion = clamp(fluidJarDistortion, 0.0, 10.0);
    float maximumDisplacement = fluidJarRefraction * distortion;
    vec2 displacementPixels = distortion * effect * (mask * interiorPixels + edge * outwardNormal * edgePixels);
    float sourceEdgeDistance = min(min(sourcePosition.x, sourceSize.x - sourcePosition.x), min(sourcePosition.y, sourceSize.y - sourcePosition.y));
    displacementPixels *= smoothstep(0.0, maximumDisplacement + 1.0, sourceEdgeDistance);
    displacementPixels /= max(1.0, length(displacementPixels) / max(maximumDisplacement, 0.001));
    displacementPixels = logicalToOutputVector(displacementPixels);

    vec3 surfaceNormal = normalize(vec3(-gradient * 20.0, 1.0));
    float oneMinusNV = 1.0 - max(surfaceNormal.z, 0.0);
    float fresnel = 0.0204 + 0.9796 * pow(oneMinusNV, 5.0);

    vec2 halfTexel = 0.5 / sourceSize;
    vec2 refractedUV = clamp(v_texcoord + displacementPixels / sourceSize, halfTexel, vec2(1.0) - halfTexel);
    vec4 refracted = texture(sharpTex, refractedUV);
    float sharpAmount = effect * mask * mix(0.98, 0.78, fresnel);

    float outputAlpha = blurred.a;
    vec3 blurredLinear = toLinearRGB(blurred.rgb / max(blurred.a, 0.001), fluidJarTransferFunction);
    vec3 refractedLinear = toLinearRGB(refracted.rgb / max(refracted.a, 0.001), fluidJarTransferFunction);
    vec3 liquidLinear = mix(blurredLinear, refractedLinear, sharpAmount);

    vec3 tintLinear = toLinearRGB(fluidJarColor.rgb, CM_TRANSFER_FUNCTION_SRGB);
    float opticalDepth = -log(max(1.0 - opacity, 0.0001));
    float thickness = clamp(fluidJarStrength, 0.0, 1.0) * mask * mix(0.25, 1.0, field);
    float transmission = exp(-opticalDepth * thickness);
    liquidLinear = liquidLinear * transmission + tintLinear * (1.0 - transmission);

    const vec2 LIGHT_DIRECTION = vec2(-0.451219, 0.892413);
    vec3 light = normalize(vec3(LIGHT_DIRECTION, 0.75));
    vec3 halfway = normalize(light + vec3(0.0, 0.0, 1.0));
    float specular = pow(max(dot(surfaceNormal, halfway), 0.0), 32.0);
    float directional = max(dot(outwardNormal, LIGHT_DIRECTION), 0.0);
    float highlight = effect * edge * (0.04 + 0.35 * fresnel + 0.18 * specular + 0.04 * directional);
    liquidLinear += mix(vec3(1.0), tintLinear, 0.12) * highlight;

    return fromLinear(vec4(liquidLinear * outputAlpha, outputAlpha), fluidJarTransferFunction);
}

void main() {
    vec4 color = texture(tex, v_texcoord);
    if (fluidJarEnabled != 0) {
        vec2 sourceSize = vec2(textureSize(tex, 0));
        vec2 position = v_texcoord * sourceSize;
        vec2 outputUV = (position - fluidJarExtent.xy) / fluidJarExtent.zw;
        if (all(greaterThanEqual(outputUV, vec2(0.0))) && all(lessThanEqual(outputUV, vec2(1.0)))) {
            vec2 logicalUV = outputToLogical(outputUV);
            vec2 fluidUV = logicalUV;
            fluidUV.y = 1.0 - fluidUV.y;
            color = applyLiquid(color, fluidUV, sourceSize, position, logicalUV * fluidJarLogicalSize);
        }
    }

    fragColor = blurFinish(color, v_texcoord, noise, brightness
#if USE_CM
                           ,
                           sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
