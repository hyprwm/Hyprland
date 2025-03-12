#version 320 es
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float contrast;
uniform float brightness;

uniform int skipCM;
uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction 
uniform mat4x2 sourcePrimaries;
uniform mat4x2 targetPrimaries;

#include "CM.glsl"

float gain(float x, float k) {
    float a = 0.5 * pow(2.0 * ((x < 0.5) ? x : 1.0 - x), k);
    return (x < 0.5) ? a : 1.0 - a;
}

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = texture(tex, v_texcoord);

    if (skipCM == 0)
        pixColor = doColorManagement(pixColor, sourceTF, sourcePrimaries, targetTF, targetPrimaries);

    // contrast
    if (contrast != 1.0) {
        pixColor.r = gain(pixColor.r, contrast);
        pixColor.g = gain(pixColor.g, contrast);
        pixColor.b = gain(pixColor.b, contrast);
    }

    // brightness
    if (brightness > 1.0) {
        pixColor.rgb *= brightness;
    }

    fragColor = pixColor;
}
