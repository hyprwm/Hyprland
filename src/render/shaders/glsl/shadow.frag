#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

#include "defines.h"

precision     highp float;
in vec4       v_color;
in vec2       v_texcoord;

uniform vec4  colorSRGB;
uniform vec2  topLeft;
uniform vec2  bottomRight;
uniform vec2  windowTopLeft;
uniform vec2  windowBottomRight;
uniform vec2  fullSize;
uniform float radius;
uniform float roundingPower;
uniform float range;
uniform float shadowPower;
uniform float thick;

#include "shadow.glsl"

layout(location = 0) out vec4 fragColor;
#if USE_MIRROR
layout(location = 1) out vec4 mirrorColor;
#endif
void main() {
    vec4 pixColor = v_color;
#if USE_MIRROR
    vec4[2] pixColors =
#else
    fragColor =
#endif
    getShadow(pixColor, colorSRGB, v_texcoord, radius, roundingPower, topLeft, fullSize, range, shadowPower, bottomRight, windowTopLeft, windowBottomRight, thick);
#if USE_MIRROR
    fragColor   = pixColors[0];
    mirrorColor = pixColors[1];
#endif
}
