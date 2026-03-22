#version 450
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
layout(location = 0) in vec2 v_texcoord;

#include "defines.h"
#include "constants.h"
#include "structs.h"

layout(push_constant, row_major) uniform UBOPush {
    layout(offset = 80) float fullSizeUntransformed[2];
    float                     radiusOuter;
    float                     thick;
    float                     angle;
    float                     angle2;
    float                     alpha;
    float                     _junk;
#if USE_ROUNDING
    SRounding rounding;
#endif
#if USE_CM
    SShaderCM cm;
#endif
#if USE_TONEMAP
    SShaderTonemap tonemap;
#endif
#if USE_TONEMAP || USE_SDR_MOD
    SShaderTargetPrimaries targetPrimaries;
#endif
}
data;

layout(std140, set = 0, binding = 0) uniform UBO {
    vec4  gradient[10];
    vec4  gradient2[10];
    int   gradientLength;
    int   gradient2Length;
    float gradientLerp;
}
gradientData;

#include "border.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = getBorder(v_texcoord, data.alpha, float2vec(data.fullSizeUntransformed), data.radiusOuter, data.thick, data.rounding.radius, data.rounding.power,
                          float2vec(data.rounding.topLeft), float2vec(data.rounding.fullSize), gradientData.gradientLength, gradientData.gradient, data.angle,
                          gradientData.gradient2Length, gradientData.gradient2, data.angle2, gradientData.gradientLerp
#if USE_CM
                          ,
                          data.cm.sourceTF, data.cm.targetTF, float33TOmat3(data.cm.convertMatrix), float2vec(data.cm.srcTFRange), float2vec(data.cm.dstTFRange)
#if USE_TONEMAP || USE_SDR_MOD
                                                                                                                                       ,
                          float33TOmat3(data.targetPrimaries.xyz)
#endif
#if USE_TONEMAP
                              ,
                          data.tonemap.maxLuminance, data.tonemap.dstMaxLuminance, data.tonemap.dstRefLuminance, data.cm.srcRefLuminance
#endif
#if USE_SDR_MOD
                          ,
                          data.targetPrimaries.sdrSaturation, data.targetPrimaries.sdrBrightnessMultiplier
#endif
#endif

    );
}
