#version 300 es

#extension GL_ARB_shading_language_include : enable
precision         highp float;
in vec2           v_texcoord; // is in 0-1
uniform sampler2D tex;
uniform float     alpha;

#include "rounding.glsl"
#include "capture.glsl"

uniform int   discardOpaque;
uniform int   discardAlpha;
uniform float discardAlphaValue;

uniform int   applyTint;
uniform vec3  tint;

layout(location = 0) out vec4 fragColor;
void main() {

    vec4 pixColor = texture(tex, v_texcoord);

    if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
        discard;

    if (discardAlpha == 1 && pixColor[3] <= discardAlphaValue)
        discard;

    if (applyTint == 1) {
        pixColor[0] = pixColor[0] * tint[0];
        pixColor[1] = pixColor[1] * tint[1];
        pixColor[2] = pixColor[2] * tint[2];
    }

    if (radius > 0.0)
        pixColor = rounding(pixColor);

    vec4 outColor = pixColor * alpha;
    fragColor     = outColor;
    CAPTURE_WRITE(outColor);
}
