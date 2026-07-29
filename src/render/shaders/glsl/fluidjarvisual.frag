#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
precision highp int;
precision highp usampler2D;

in vec2 v_texcoord;
uniform sampler2D fluidJarParticleTex;
uniform usampler2D fluidJarGraphTex;
uniform usampler2D fluidJarTrackingTex;
uniform sampler2D fluidJarVisualTex;
uniform vec2 fluidJarResolution;
uniform vec2 fluidJarGridSize;
uniform int fluidJarParticleCount;
uniform float fluidJarVisualResponse;

#include "fluidJar.glsl"

layout(location = 0) out vec4 fragColor;

const float FIELD_FALLOFF = 0.055;

vec2 samplePosition;
float fieldSum = 0.0;
float weightSum = 0.0;
vec2 velocitySum = vec2(0.0);
float materialSum = 0.0;

float shapedDistanceSquared(vec2 delta, vec2 shapeSeed) {
    float distanceSquared = dot(delta, delta);
    if (distanceSquared < 1e-6)
        return 0.0;

    vec2 axis = 2.0 * shapeSeed - 1.0;
    axis *= inversesqrt(max(dot(axis, axis), 0.01));
    vec2 direction = delta * inversesqrt(distanceSquared);
    float along = dot(direction, axis);
    float across = dot(direction, vec2(-axis.y, axis.x));
    float thirdOrder = 4.0 * along * along * along - 3.0 * along;
    float radialScale = clamp(1.0 + 0.10 * along + 0.06 * (2.0 * across * across - 1.0) + 0.035 * thirdOrder, 0.80, 1.20);
    return distanceSquared / (radialScale * radialScale);
}

void addParticle(int id, ivec2 gridSize) {
    if (id < 0 || id >= fluidJarParticleCount)
        return;

    vec4 state = texelFetch(fluidJarParticleTex, fluidJarAddress(id, 0, gridSize), 0);
    vec4 material = texelFetch(fluidJarParticleTex, fluidJarAddress(id, 2, gridSize), 0);
    vec2 delta = samplePosition - state.xy;
    float contribution = exp(-FIELD_FALLOFF * shapedDistanceSquared(delta, material.zw));
    fieldSum += contribution;
    weightSum += contribution;
    velocitySum += state.zw * contribution;
    float materialValue = dot(2.0 * material - 1.0, vec4(0.40, 0.25, 0.22, 0.13));
    materialSum += materialValue * contribution;
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 gridSize = ivec2(fluidJarGridSize);
    int id = fluidJarDecodeId(texelFetch(fluidJarTrackingTex, pixel, 0).rg);
    vec4 previous = texelFetch(fluidJarVisualTex, pixel, 0);
    if (id < 0 || id >= fluidJarParticleCount) {
        fragColor = mix(previous, vec4(0.0), 0.65);
        return;
    }

    samplePosition = vec2(pixel) + vec2(0.5);
    addParticle(id, gridSize);
    for (int direction = 0; direction < 4; ++direction) {
        ivec4 neighbors = fluidJarLoadNeighbors(fluidJarGraphTex, id, direction, gridSize);
        for (int index = 0; index < 4; ++index)
            addParticle(neighbors[index], gridSize);
    }

    float field = 1.0 - exp(-fieldSum);
    vec2 flow = weightSum > 1e-5 ? velocitySum / (1.5 * weightSum) : vec2(0.0);
    float heterogeneity = weightSum > 1e-5 ? clamp(materialSum / weightSum, -1.0, 1.0) : 0.0;
    float presence = smoothstep(0.03, 0.2, field);
    vec4 current = vec4(clamp(flow, vec2(-1.0), vec2(1.0)) * presence, heterogeneity * presence, clamp(field, 0.0, 1.0));

    vec3 response = vec3(1.0) - pow(vec3(0.55, 0.45, 0.15), vec3(max(fluidJarVisualResponse, 1.0)));
    fragColor.rg = mix(previous.rg, current.rg, response.x);
    fragColor.b = mix(previous.b, current.b, response.y);
    fragColor.a = mix(previous.a, current.a, response.z);
}
