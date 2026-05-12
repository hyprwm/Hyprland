#version 300 es

#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#include "defines.h"

precision highp float;
in vec4   v_color;

uniform vec4 colorSRGB;
#if USE_ROUNDING
uniform float radius;
uniform float roundingPower;
uniform vec2  topLeft;
uniform vec2  fullSize;
#include "rounding.glsl"
#endif

layout(location = 0) out vec4 fragColor;
#if USE_MIRROR
layout(location = 1) out vec4 mirrorColor;
#endif
void main() {
    vec4 pixColor = v_color;

#if USE_ROUNDING
    pixColor = rounding(pixColor, radius, roundingPower, topLeft, fullSize);
#endif

    fragColor = pixColor;
#if USE_MIRROR
#if USE_ROUNDING
    mirrorColor = rounding(colorSRGB, radius, roundingPower, topLeft, fullSize);
#else
    mirrorColor = colorSRGB;
#endif
#endif
}
