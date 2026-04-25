#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

#include "defines.h"

precision         highp float;
in vec2           v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float     contrast;
uniform float     brightness;

uniform int       sourceTF; // eTransferFunction
uniform int       targetTF; // eTransferFunction

#if USE_CM
uniform vec2  srcTFRange;
uniform vec2  dstTFRange;

uniform float srcRefLuminance;
uniform mat3  convertMatrix;

uniform float sdrBrightnessMultiplier;
#include "cm_helpers.glsl"
#endif

#include "blurprepare.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = fragColor = blurPrepare(texture(tex, v_texcoord), contrast, brightness
#if USE_CM
                                        ,
                                        sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange, srcRefLuminance, sdrBrightnessMultiplier
#endif
    );
}
