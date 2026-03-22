#version 450
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant, row_major) uniform UBO {
    layout(offset = 80) float radius;
    vec2                      halfpixel;
    int                       passes;
    float                     vibrancy;
    float                     vibrancy_darkness;
}
data;

layout(location = 0) out vec4 fragColor;

#include "blur1.glsl"

void main() {
    fragColor = blur1(v_texcoord, tex, data.radius, data.halfpixel, data.passes, data.vibrancy, data.vibrancy_darkness);
}
