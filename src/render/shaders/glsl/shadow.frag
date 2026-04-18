#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

#include "defines.h"

precision     highp float;
in vec4       v_color;
in vec2       v_texcoord;

uniform int   sourceTF; // eTransferFunction
uniform int   targetTF; // eTransferFunction
uniform mat3  targetPrimariesXYZ;

uniform vec2  topLeft;
uniform vec2  bottomRight;
uniform vec2  fullSize;
uniform float radius;
uniform float roundingPower;
uniform float range;
uniform float shadowPower;
uniform float thick;

#if USE_CM
#include "cm_helpers.glsl"
#include "CM.glsl"
#endif

#include "shadow.glsl"

layout(location = 0) out vec4 fragColor;
#if USE_MIRROR
layout(location = 1) out vec4 mirrorColor;
#endif
void main() {
    vec4 pixColor = v_color;
#if USE_MIRROR
    vec4[2] pixColors =
#else
    fragColor =
#endif
        getShadow(pixColor, v_texcoord, radius, roundingPower, topLeft, fullSize, range, shadowPower, bottomRight, thick
#if USE_CM
                  ,
                  sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#if USE_ICC
                  ,
                  iccLut3D, iccLutSize
#else
#if USE_TONEMAP || USE_SDR_MOD
                  ,
                  targetPrimariesXYZ
#endif
#if USE_TONEMAP
                  ,
                  maxLuminance, dstMaxLuminance, dstRefLuminance, srcRefLuminance
#endif
#if USE_SDR_MOD
                  ,
                  sdrSaturation, sdrBrightnessMultiplier
#endif
#endif
#endif
        );
#if USE_MIRROR
    fragColor   = pixColors[0];
    mirrorColor = pixColors[1];
#endif
}