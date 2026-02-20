#version 300 es
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float contrast;
uniform float brightness;

uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction

#include "CM.glsl"
#include "gain.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = texture(tex, v_texcoord);

    if (sourceTF == CM_TRANSFER_FUNCTION_ST2084_PQ) {
        pixColor.rgb /= sdrBrightnessMultiplier;
    }
    pixColor.rgb = convertMatrix * toLinearRGB(pixColor.rgb, sourceTF);
    pixColor = toNit(pixColor, vec2(srcTFRange[0], srcRefLuminance));
    pixColor = fromLinearNit(pixColor, targetTF, dstTFRange);

    // contrast
    if (contrast != 1.0)
        pixColor.rgb = gain(pixColor.rgb, contrast);

    // brightness
    pixColor.rgb *= max(1.0, brightness);

    fragColor = pixColor;
}
