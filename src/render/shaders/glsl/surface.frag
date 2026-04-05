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

#if USE_ANIMATION
uniform float animationProgress;
uniform int   isClosingAnimation;
uniform float animationSeed;
#endif

#if USE_ROUNDING || USE_ANIMATION
uniform vec2 topLeft;
uniform vec2 fullSize;
#endif

#if USE_ROUNDING
uniform float radius;
uniform float roundingPower;
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

#if USE_ANIMATION
#include "openClose.glsl"
#endif

layout(location = 0) out vec4 fragColor;
#if USE_MIRROR
layout(location = 1) out vec4 mirrorColor;
#endif
void main() {
#if USE_ANIMATION
    vec4[2] animated      = isClosingAnimation == 1 ? closeAnimation(tex, v_texcoord, fullSize, 1.0 - animationProgress, animationSeed) :
                                                      openAnimation(tex, v_texcoord, fullSize, animationProgress, animationSeed);
    vec4  pixColor        = animated[0];
    vec2  texCoord        = animated[1].xy;
    float additionalAlpha = animated[1].a;
#else
    vec2  texCoord        = v_texcoord;
    float additionalAlpha = 1.0;
#if USE_RGBA
    vec4 pixColor = texture(tex, v_texcoord);
#else
    vec4 pixColor = vec4(texture(tex, v_texcoord).rgb, 1.0);
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
        doColorManagement(pixColor, sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
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
    pixColor    = pixColors[0];
    mirrorColor = pixColors[1];
#endif

#if USE_TINT
    pixColor.rgb = pixColor.rgb * tint;
#endif

#if USE_ROUNDING
    pixColor = rounding(pixColor, radius, roundingPower, topLeft, fullSize);
#endif
#if USE_BLUR
#if USE_DISCARD
    pixColor = mix(pixColor, vec4(mix(texture(blurredBG, texCoord * uvSize + uvOffset).rgb, pixColor.rgb, pixColor.a), 1.0),
                   discardAlpha && (pixColor.a <= discardAlphaValue) ? 0.0 : 1.0);
#else
    pixColor = vec4(mix(texture(blurredBG, texCoord * uvSize + uvOffset).rgb, pixColor.rgb, pixColor.a), 1.0);
#endif
#endif

    fragColor = pixColor * alpha * additionalAlpha;
#if USE_MIRROR
#if USE_TINT
    mirrorColor.rgb = mirrorColor.rgb * tint;
#endif

#if USE_ROUNDING
    mirrorColor = rounding(mirrorColor, radius, roundingPower, topLeft, fullSize);
#endif
#if USE_BLUR
#if USE_DISCARD
    mirrorColor = mix(mirrorColor, vec4(mix(texture(blurredBG, texCoord * uvSize + uvOffset).rgb, mirrorColor.rgb, mirrorColor.a), 1.0),
                      discardAlpha && (mirrorColor.a <= discardAlphaValue) ? 0.0 : 1.0);
#else
    mirrorColor = vec4(mix(texture(blurredBG, texCoord * uvSize + uvOffset).rgb, mirrorColor.rgb, mirrorColor.a), 1.0);
#endif
#endif

    mirrorColor = mirrorColor * alpha * additionalAlpha;
#endif
}
