#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

#include "defines.h"

precision         highp float;
in vec2           v_texcoord;
uniform sampler2D tex;
#if USE_BLUR
uniform vec2      uvSize;
uniform vec2      uvOffset;
uniform sampler2D blurredBG;
#endif

uniform float alpha;

#if USE_DISCARD
uniform bool  discardOpaque;
uniform bool  discardAlpha;
uniform float discardAlphaValue;
#endif

#if USE_TINT
uniform vec3 tint;
#endif

#if USE_ROUNDING
uniform float radius;
uniform float roundingPower;
uniform vec2  topLeft;
uniform vec2  fullSize;
#include "rounding.glsl"
#endif

#if USE_CM
uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction

#if USE_TONEMAP || USE_SDR_MOD
uniform mat3 targetPrimariesXYZ;
#else
const mat3 targetPrimariesXYZ = mat3(0.0);
#endif

#include "CM.glsl"
#endif

layout(location = 0) out vec4 fragColor;
void main() {
#if USE_RGBA
    vec4 pixColor = texture(tex, v_texcoord);
#else
    vec4 pixColor = vec4(texture(tex, v_texcoord).rgb, 1.0);
#endif

#if USE_DISCARD && !USE_BLUR
    if (discardOpaque && pixColor.a * alpha == 1.0)
        discard;

    if (discardAlpha && pixColor.a <= discardAlphaValue)
        discard;
#endif

#if USE_CM
    pixColor = doColorManagement(pixColor, sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
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
    );
#endif

#if USE_TINT
    pixColor.rgb = pixColor.rgb * tint;
#endif

#if USE_ROUNDING
    pixColor = rounding(pixColor, radius, roundingPower, topLeft, fullSize);
#endif
#if USE_BLUR
#if USE_DISCARD
    pixColor = mix(pixColor, vec4(mix(texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb, pixColor.rgb, pixColor.a), 1.0),
                   discardAlpha && (pixColor.a <= discardAlphaValue) ? 0.0 : 1.0);
#else
    pixColor = vec4(mix(texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb, pixColor.rgb, pixColor.a), 1.0);
#endif
#endif

    fragColor = pixColor * alpha;
}
