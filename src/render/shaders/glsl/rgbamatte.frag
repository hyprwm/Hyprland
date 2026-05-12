#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

#include "defines.h"

precision         highp float;
in vec2           v_texcoord; // is in 0-1
uniform sampler2D tex;
uniform sampler2D texMatte;

layout(location = 0) out vec4 fragColor;
#if USE_MIRROR
layout(location = 1) out vec4 mirrorColor;
#endif
void main() {
    fragColor = texture(tex, v_texcoord) * texture(texMatte, v_texcoord)[0]; // I know it only uses R, but matte should be black/white anyways.
#if USE_MIRROR
    mirrorColor = fragColor;
#endif
}
