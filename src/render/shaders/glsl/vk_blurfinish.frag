#version 450
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant, row_major) uniform UBO {
    layout(offset = 80) float noise;
    float                     brightness;
}
data;

#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = texture(tex, v_texcoord);

    fragColor = blurFinish(pixColor, v_texcoord, data.noise, data.brightness);
}
