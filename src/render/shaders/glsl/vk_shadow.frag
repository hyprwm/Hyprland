#version 450
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
layout(location = 0) in vec2 v_texcoord;

#include "defines.h"
#include "constants.h"
#include "structs.h"

layout(push_constant, row_major) uniform UBO {
    layout(offset = 80) vec4 v_color;
    vec2                     bottomRight;
    float                    range;
    float                    shadowPower;
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

#include "shadow.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = getShadow(data.v_color, v_texcoord, data.rounding.radius, data.rounding.power, float2vec(data.rounding.topLeft), float2vec(data.rounding.fullSize), data.range,
                          data.shadowPower, data.bottomRight
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