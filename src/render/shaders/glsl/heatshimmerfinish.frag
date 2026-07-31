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
uniform float     time;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "glassFinish.glsl"

layout(location = 0) out vec4 fragColor;

vec2 heatShimmerNormal(vec2 position) {
    vec2 warpedPosition = position;
    warpedPosition.x += sin(position.y * 0.73 - time) * 0.32;
    warpedPosition.y += sin(position.x * 0.41 + time) * 0.12;

    float broadPhase  = warpedPosition.y * 1.15 + warpedPosition.x * 0.22 + time;
    float detailPhase = warpedPosition.y * 2.37 - warpedPosition.x * 0.31 + time * 2.0;
    float crossPhase  = warpedPosition.x * 0.74 + warpedPosition.y * 0.41 - time;

    return vec2(cos(broadPhase) * 0.46 + cos(detailPhase) * 0.22 + sin(crossPhase) * 0.09,
                cos(crossPhase) * 0.09 + sin(detailPhase) * 0.05);
}

void main() {
    vec2 position = (gl_FragCoord.xy - glassPosition) / glassSize;
    fragColor = glassFinish(heatShimmerNormal(position));
}
