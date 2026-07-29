#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;
in vec2           v_texcoord;
uniform sampler2D tex;

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

#include "glassFinish.glsl"

layout(location = 0) out vec4 fragColor;

vec2 frostRandom(vec2 cell) {
    return vec2(hash(cell + vec2(13.37, 71.91)), hash(cell + vec2(83.17, 29.53)));
}

vec2 frostWarp(vec2 position) {
    const vec2 DIRECTION_1 = vec2(1.73, -1.21);
    const vec2 DIRECTION_2 = vec2(1.11, 1.87);
    const vec2 DIRECTION_3 = vec2(-2.19, 0.83);

    vec2 warp = vec2(sin(dot(position, DIRECTION_1) + 0.7), cos(dot(position, DIRECTION_2) + 1.9));
    warp += vec2(cos(dot(position, DIRECTION_3) + 2.8), sin(dot(position, DIRECTION_1 - DIRECTION_2) + 4.1)) * 0.45;
    return warp * 0.16;
}

void frostCellular(vec2 position, out vec2 nearestOffset, out vec2 secondOffset) {
    vec2  baseCell       = floor(position);
    float nearestDistance = 1e10;
    float secondDistance  = 1e10;

    nearestOffset = vec2(0.0);
    secondOffset  = vec2(0.0);

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 cell   = baseCell + vec2(float(x), float(y));
            vec2 center = cell + mix(vec2(0.16), vec2(0.84), frostRandom(cell));
            vec2 offset = position - center;
            float distance = dot(offset, offset);

            if (distance < nearestDistance) {
                secondDistance = nearestDistance;
                secondOffset   = nearestOffset;
                nearestDistance = distance;
                nearestOffset   = offset;
            } else if (distance < secondDistance) {
                secondDistance = distance;
                secondOffset   = offset;
            }
        }
    }
}

vec2 frostGradient(vec2 position) {
    position += frostWarp(position);

    vec2 nearestOffset;
    vec2 secondOffset;
    frostCellular(position, nearestOffset, secondOffset);

    float boundary = length(secondOffset) - length(nearestOffset);
    float seamWidth = 0.028 + fwidth(boundary) * 1.5;
    float seam = 1.0 - smoothstep(0.0, seamWidth, boundary);

    vec2 seamDirection = secondOffset - nearestOffset;
    seamDirection /= max(length(seamDirection), 0.0001);

    vec2 grain = vec2(sin(dot(position, vec2(2.61, -1.43))), cos(dot(position, vec2(1.19, 2.37)))) * 0.09;
    return nearestOffset * 0.34 + grain + seamDirection * seam * 0.55;
}

void main() {
    vec2 position = (gl_FragCoord.xy - glassPosition) / glassSize;
    vec2 gradient = frostGradient(position);
    fragColor = glassFinish(gradient);
}
