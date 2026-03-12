#version 450
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

#include "defines.h"
#include "constants.h"
#include "structs.h"

layout(push_constant, row_major) uniform UBO {
    layout(offset = 80) float contrast;
    float                     brightness;
    float                     sdrBrightnessMultiplier;
#if USE_CM
    layout(offset = 96) SShaderCM cm;
#endif
}
data;

layout(location = 0) out vec4 fragColor;
#include "blurprepare.glsl"

void main() {
    fragColor = blurPrepare(texture(tex, v_texcoord), data.contrast, data.brightness
#if USE_CM
                            ,
                            data.cm.sourceTF, data.cm.targetTF, float33TOmat3(data.cm.convertMatrix), float2vec(data.cm.srcTFRange), float2vec(data.cm.dstTFRange),
                            data.cm.srcRefLuminance, data.sdrBrightnessMultiplier
#endif
    );
}
