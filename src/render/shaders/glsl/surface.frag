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
uniform float     blurAlpha;
#endif
#if USE_BLUR_MATTE
uniform sampler2D blurAlphaMatte;
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
#else
const float radius        = 0.0;
const float roundingPower = 2.0;
#endif

#if USE_MOTION_BLUR
uniform vec4  motionPrevBox;
uniform vec4  motionCurrBox;
uniform vec4  motionSourceBox;
uniform vec2  motionSourceTexSize;
uniform int   motionSamples;
#include "motion_blur.glsl"
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
#if USE_MIRROR
layout(location = 1) out vec4 mirrorColor;
#endif
void main() {
#if USE_MOTION_BLUR
    vec4 pixColor = motionBlurSample(tex, motionPrevBox, motionCurrBox, motionSourceBox, motionSourceTexSize, motionSamples, USE_RGBA == 1);
#if USE_BLUR_MATTE
    float blurAlphaMask = clamp(motionBlurSample(blurAlphaMatte, motionPrevBox, motionCurrBox, motionSourceBox, motionSourceTexSize, motionSamples, true).r, 0.0, 1.0);
#endif
#else
#if USE_RGBA
    vec4 pixColor = texture(tex, v_texcoord);
#else
    vec4 pixColor = vec4(texture(tex, v_texcoord).rgb, 1.0);
#endif
#if USE_BLUR_MATTE
    float blurAlphaMask = clamp(texture(blurAlphaMatte, v_texcoord).r, 0.0, 1.0);
#endif
#endif

#if USE_DISCARD && !USE_BLUR
    if (discardOpaque && pixColor.a * alpha == 1.0)
        discard;

    if (discardAlpha && pixColor.a <= discardAlphaValue)
        discard;
#endif

#if USE_CM
#if USE_MIRROR
    vec4[2] pixColors =
#else
    pixColor =
#endif
        doColorManagement(pixColor, alpha, sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
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
#if USE_MIRROR
#if USE_CM
    pixColor    = pixColors[0];
    mirrorColor = pixColors[1];
#else
    mirrorColor = pixColor;
#endif
#endif

#if USE_TINT
    pixColor.rgb = pixColor.rgb * tint;
#endif

#if USE_ROUNDING && !USE_MOTION_BLUR
    pixColor = rounding(pixColor, radius, roundingPower, topLeft, fullSize);
#endif
#if !USE_CM
    pixColor *= alpha;
#endif
#if USE_BLUR
#if USE_BLUR_MATTE
    float pixBlurAlphaMask = blurAlphaMask * blurAlpha;
#if USE_DISCARD
    if (discardAlpha && pixColor.a <= discardAlphaValue)
        pixBlurAlphaMask = 0.0;
#endif
    vec3 blurredPixColor = texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb;
    float pixBlurBgAlpha = (1.0 - pixColor.a) * pixBlurAlphaMask;
    pixColor             = vec4(pixColor.rgb + blurredPixColor * pixBlurBgAlpha, pixColor.a + pixBlurBgAlpha);
#else
#if USE_BLUR_ALPHA_MASK
    if (pixColor.a <= 0.0)
        discard;
#endif
#if USE_DISCARD
    float pixBlurAlphaMask = discardAlpha && (pixColor.a <= discardAlphaValue) ? 0.0 : 1.0;
#else
    float pixBlurAlphaMask = 1.0;
#endif
    vec3 blurredPixColor = texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb;
    float pixBlurBgAlpha = (1.0 - pixColor.a) * pixBlurAlphaMask;
    pixColor             = vec4(pixColor.rgb + blurredPixColor * pixBlurBgAlpha, pixColor.a + pixBlurBgAlpha);
#endif
#endif

    fragColor = pixColor;
#if USE_MIRROR
#if USE_TINT
    mirrorColor.rgb = mirrorColor.rgb * tint;
#endif

#if USE_ROUNDING && !USE_MOTION_BLUR
    mirrorColor = rounding(mirrorColor, radius, roundingPower, topLeft, fullSize);
#endif
#if !USE_CM
    mirrorColor *= alpha;
#endif
#if USE_BLUR
#if USE_BLUR_MATTE
    float mirrorBlurAlphaMask = blurAlphaMask * blurAlpha;
#if USE_DISCARD
    if (discardAlpha && mirrorColor.a <= discardAlphaValue)
        mirrorBlurAlphaMask = 0.0;
#endif
    vec3 blurredMirrorColor = texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb;
    float mirrorBlurBgAlpha = (1.0 - mirrorColor.a) * mirrorBlurAlphaMask;
    mirrorColor             = vec4(mirrorColor.rgb + blurredMirrorColor * mirrorBlurBgAlpha, mirrorColor.a + mirrorBlurBgAlpha);
#else
#if USE_BLUR_ALPHA_MASK
    if (mirrorColor.a > 0.0) {
#endif
#if USE_DISCARD
        float mirrorBlurAlphaMask = discardAlpha && (mirrorColor.a <= discardAlphaValue) ? 0.0 : 1.0;
#else
        float mirrorBlurAlphaMask = 1.0;
#endif
        vec3 blurredMirrorColor = texture(blurredBG, v_texcoord * uvSize + uvOffset).rgb;
        float mirrorBlurBgAlpha = (1.0 - mirrorColor.a) * mirrorBlurAlphaMask;
        mirrorColor             = vec4(mirrorColor.rgb + blurredMirrorColor * mirrorBlurBgAlpha, mirrorColor.a + mirrorBlurBgAlpha);
#if USE_BLUR_ALPHA_MASK
    } else
        mirrorColor = vec4(0.0);
#endif
#endif
#endif

#endif
}
