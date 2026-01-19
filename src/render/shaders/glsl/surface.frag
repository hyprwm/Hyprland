#version 300 es
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord;
uniform sampler2D tex;

uniform float alpha;

#include "discard.glsl"
#include "tint.glsl"
#include "rounding.glsl"
#include "surface_CM.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    #include "get_rgb_pixel.glsl"

    #include "do_discard.glsl"
    #include "do_CM.glsl"
    #include "do_tint.glsl"
    #include "do_rounding.glsl"

    fragColor = pixColor * alpha;
}
