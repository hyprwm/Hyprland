#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

#include "defines.h"

precision         highp float;
in vec2           v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float     contrast;
uniform float     brightness;

#include "blurprepare.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = blurPrepare(texture(tex, v_texcoord), contrast, brightness);
}
