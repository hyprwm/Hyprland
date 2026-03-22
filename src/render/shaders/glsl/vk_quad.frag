#version 450
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
layout(location = 0) in vec2 v_texcoord;

#include "defines.h"
#include "constants.h"
#include "structs.h"

layout(push_constant, row_major) uniform UBO {
    layout(offset = 80) vec4 v_color;
#if USE_ROUNDING
    SRounding rounding;
#endif
}
data;

#if USE_ROUNDING
#include "rounding.glsl"
#endif

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = data.v_color;

#if USE_ROUNDING
    pixColor = rounding(pixColor, data.rounding.radius, data.rounding.power, float2vec(data.rounding.topLeft), float2vec(data.rounding.fullSize));
#endif

    fragColor = pixColor;
}
