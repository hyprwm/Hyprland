#version 320 es
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float noise;
uniform float brightness;

uniform int skipCM;
uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction 
uniform mat4x2 sourcePrimaries;
uniform mat4x2 targetPrimaries;

#include "CM.glsl"

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 1689.1984);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = texture(tex, v_texcoord);

    // noise
    float noiseHash   = hash(v_texcoord);
    float noiseAmount = (mod(noiseHash, 1.0) - 0.5);
    pixColor.rgb += noiseAmount * noise;

    // brightness
    if (brightness < 1.0) {
        pixColor.rgb *= brightness;
    }

    if (skipCM == 0)
        pixColor = doColorManagement(pixColor, sourceTF, sourcePrimaries, targetTF, targetPrimaries);

    fragColor = pixColor;
}
