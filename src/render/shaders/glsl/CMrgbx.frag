#version 300 es
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord;
uniform sampler2D tex;

uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction
uniform mat3 targetPrimariesXYZ;

uniform float alpha;
uniform bool applyTint;
uniform vec3 tint;

#include "rounding.glsl"
#include "CM.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = vec4(texture(tex, v_texcoord).rgb, 1.0);

    // this shader shouldn't be used when skipCM == 1
    pixColor = doColorManagement(pixColor, sourceTF, targetTF, targetPrimariesXYZ);

    if (applyTint)
        pixColor.rgb *= tint;

    if (radius > 0.0)
        pixColor = rounding(pixColor);

    fragColor = pixColor * alpha;
}
