#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;
in vec2           v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float     noise;
uniform float     brightness;

#include "defines.h"
#if USE_CM
uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction
#include "CM.glsl"
#endif

#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = texture(tex, v_texcoord);

    fragColor = blurFinish(pixColor, v_texcoord, noise, brightness
#if USE_CM
                           ,
                           sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
