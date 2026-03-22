#version 450
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_debug_printf : enable

layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 1, binding = 0) uniform sampler2D blurredBG;

layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec2 uvOffset;
layout(location = 2) in vec2 uvSize;
layout(location = 0) out vec4 fragColor;

#include "defines.h"
#include "constants.h"
#include "structs.h"

layout(push_constant, row_major) uniform UBO {
    layout(offset = 80) float alpha;
    float                     tint;
    int                       discardMode;
    float                     discardAlphaValue;
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

#if USE_CM
#include "cm_helpers.glsl"
#endif

#if USE_ROUNDING
#include "rounding.glsl"
#endif

void main() {
#if USE_RGBA
    vec4 pixColor = texture(tex, v_texcoord);
#else
    vec4 pixColor = vec4(texture(tex, v_texcoord).rgb, 1.0);
#endif

#if USE_DISCARD && !USE_BLUR
    if ((data.discardMode == 1 || data.discardMode == 3) && pixColor.a * data.alpha == 1.0)
        discard;

    if ((data.discardMode == 2 || data.discardMode == 3) && pixColor.a <= data.discardAlphaValue)
        discard;
#endif

#if USE_CM
    mat3 convertMatrix = float33TOmat3(data.cm.convertMatrix);
    pixColor           = doColorManagement(pixColor, data.cm.sourceTF, data.cm.targetTF, convertMatrix, float2vec(data.cm.srcTFRange), float2vec(data.cm.dstTFRange)
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
    );
#endif

#if USE_TINT
    pixColor.rgb = pixColor.rgb * data.tint;
#endif

#if USE_ROUNDING
    pixColor = rounding(pixColor, data.rounding.radius, data.rounding.power, float2vec(data.rounding.topLeft), float2vec(data.rounding.fullSize));
#endif

#if USE_BLUR
#if USE_DISCARD
    pixColor = mix(pixColor, vec4(mix(texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb, pixColor.rgb, pixColor.a), 1.0),
                   ((data.discardMode == 2 || data.discardMode == 3)) && (pixColor.a <= data.discardAlphaValue) ? 0.0 : 1.0);
#else
    pixColor = vec4(mix(texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb, pixColor.rgb, pixColor.a), 1.0);
#endif
#endif

    fragColor = pixColor * data.alpha;
}
