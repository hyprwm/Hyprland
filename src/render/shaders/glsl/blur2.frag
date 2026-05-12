#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;

uniform sampler2D tex;
uniform float     radius;
uniform vec2      halfpixel;

in vec2           v_texcoord;
layout(location = 0) out vec4 fragColor;

#include "blur2.glsl"

void main() {
    fragColor = blur2(v_texcoord, tex, radius, halfpixel);
}
