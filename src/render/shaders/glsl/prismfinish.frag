#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;
in vec2           v_texcoord;
uniform sampler2D tex;
uniform sampler2D sharpTex;

uniform float     noise;
uniform float     brightness;
uniform float     glassRefraction;
uniform float     glassSize;
uniform float     glassRoughness;
uniform vec2      glassPosition;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;

const float TAU = 6.28318530718;

float triangleWave(float phase) {
    return 1.0 - abs(fract(phase) * 2.0 - 1.0);
}

float distanceToFold(float phase) {
    phase = fract(phase);
    return min(min(phase, 1.0 - phase), abs(phase - 0.5));
}

vec2 prismSurface(vec2 position, out float edge, out float spectrumPhase) {
    const vec2 AXIS_A = vec2(1.0, 0.0);
    const vec2 AXIS_B = vec2(-0.5, 0.8660254);
    const vec2 AXIS_C = vec2(-0.5, -0.8660254);

    vec3 phase = vec3(dot(position, AXIS_A), dot(position, AXIS_B), dot(position, AXIS_C));
    vec3 slope = 1.0 - 2.0 * step(vec3(0.5), fract(phase));
    vec2 normal = (slope.x * AXIS_A + slope.y * AXIS_B + slope.z * AXIS_C) * 0.46;

    vec2 cell = floor(vec2(phase.x, phase.y));
    vec2 variation = vec2(hash(cell + vec2(19.17, 73.41)), hash(cell + vec2(61.83, 11.29))) - 0.5;
    normal += variation * 0.18;

    float foldDistance = min(distanceToFold(phase.x), min(distanceToFold(phase.y), distanceToFold(phase.z)));
    float antialias = max(fwidth(foldDistance), 0.001);
    edge = 1.0 - smoothstep(antialias * 0.65, antialias * 2.8, foldDistance);

    float height = triangleWave(phase.x) + triangleWave(phase.y) + triangleWave(phase.z);
    spectrumPhase = fract(height * 0.19 + hash(cell) * 0.24);
    return normal;
}

vec4 prismFinish(vec2 normal, float edge, float spectrumPhase) {
    normal /= max(1.0, length(normal));

    vec2 uvStep = normal.x * dFdx(v_texcoord) + normal.y * dFdy(v_texcoord);
    vec2 refraction = glassRefraction * uvStep;
    vec2 texSize = vec2(textureSize(tex, 0));
    vec2 halfTexel = 0.5 / texSize;
    vec2 minimumUV = halfTexel;
    vec2 maximumUV = vec2(1.0) - halfTexel;

    vec2 centerUV = clamp(v_texcoord + refraction * 0.72, minimumUV, maximumUV);
    float dispersion = mix(0.12, 0.28, glassRoughness);
    vec2 redUV = clamp(centerUV + refraction * dispersion, minimumUV, maximumUV);
    vec2 blueUV = clamp(centerUV - refraction * dispersion, minimumUV, maximumUV);

    vec4 blurred = texture(tex, centerUV);
    vec4 sharpCenter = texture(sharpTex, centerUV);
    vec3 dispersed = vec3(texture(sharpTex, redUV).r, sharpCenter.g, texture(sharpTex, blueUV).b);
    float clarity = smoothstep(0.0, 6.0, glassRefraction) * mix(0.24, 0.42, glassRoughness);
    vec4 pixColor = vec4(mix(blurred.rgb, dispersed, clarity), blurred.a);

    const vec2 LIGHT_DIRECTION = vec2(-0.451219, 0.892413);
    float emboss = dot(normal, LIGHT_DIRECTION);
    float highlight = edge * glassRoughness * (0.018 + 0.055 * max(emboss, 0.0));
    vec3 spectrum = 0.58 + 0.42 * cos(TAU * (spectrumPhase + vec3(0.0, 0.333333, 0.666667)));
    pixColor.rgb *= 1.0 + emboss * glassRoughness * 0.1;
    pixColor.rgb += spectrum * highlight * pixColor.a;

    return blurFinish(pixColor, v_texcoord, noise, brightness
#if USE_CM
                      ,
                      sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}

void main() {
    vec2 position = (gl_FragCoord.xy - glassPosition) / glassSize;
    float edge;
    float spectrumPhase;
    vec2 normal = prismSurface(position, edge, spectrumPhase);
    fragColor = prismFinish(normal, edge, spectrumPhase);
}
