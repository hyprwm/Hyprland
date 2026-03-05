#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision     highp float;
in vec2       v_texcoord;

uniform int   sourceTF; // eTransferFunction
uniform int   targetTF; // eTransferFunction
uniform mat3  targetPrimariesXYZ;

uniform vec2  fullSizeUntransformed;
uniform float radiusOuter;
uniform float thick;

// Gradients are in OkLabA!!!! {l, a, b, alpha}
uniform vec4  gradient[10];
uniform vec4  gradient2[10];
uniform int   gradientLength;
uniform int   gradient2Length;
uniform float angle;
uniform float angle2;
uniform float gradientLerp;
uniform float alpha;

uniform float radius;
uniform float roundingPower;
uniform vec2  topLeft;
uniform vec2  fullSize;
#include "rounding.glsl"
#include "CM.glsl"
#include "border.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = getBorder(v_texcoord, alpha, fullSizeUntransformed, radiusOuter, thick, radius, roundingPower, topLeft, fullSize, gradientLength, gradient, angle, gradient2Length,
                          gradient2, angle2, gradientLerp
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
}
