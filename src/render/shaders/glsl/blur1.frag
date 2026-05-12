#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;
uniform sampler2D tex;

uniform float     radius;
uniform vec2      halfpixel;
uniform int       passes;
uniform float     vibrancy;
uniform float     vibrancy_darkness;

in vec2           v_texcoord;
layout(location = 0) out vec4 fragColor;

#include "blur1.glsl"

void main() {
    fragColor = blur1(v_texcoord, tex, radius, halfpixel, passes, vibrancy, vibrancy_darkness);
}
