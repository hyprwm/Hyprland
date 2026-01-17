#version 300 es
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord;
uniform sampler2D tex;

uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction
uniform mat4x2 targetPrimaries;

uniform float alpha;

uniform bool discardOpaque;
uniform bool discardAlpha;
uniform float discardAlphaValue;

uniform bool applyTint;
uniform vec3 tint;

#include "rounding.glsl"
#include "CM.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = texture(tex, v_texcoord);

    if (discardOpaque && pixColor.a * alpha == 1.0)
        discard;

    if (discardAlpha && pixColor.a <= discardAlphaValue)
        discard;

    // this shader shouldn't be used when skipCM == 1
    pixColor = doColorManagement(pixColor, sourceTF, targetTF, primaries2xyz(targetPrimaries));

    if (applyTint)
        pixColor.rgb *= tint;

    if (radius > 0.0)
        pixColor = rounding(pixColor);

    fragColor = pixColor * alpha;
}
