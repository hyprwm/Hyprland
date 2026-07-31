#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;
in vec2           v_texcoord;
uniform sampler2D tex;

uniform float     noise;
uniform float     brightness;
uniform float     glassRefraction;
uniform float     glassSize;
uniform float     glassRoughness;
uniform vec2      glassPosition;
uniform float     time;
uniform float     auroraIntensity;
uniform vec4      auroraColor1;
uniform vec4      auroraColor2;
uniform int       auroraTransferFunction;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "cm_helpers.glsl"
#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;

vec3 auroraSurface(vec2 position) {
    float verticalWarp = sin(position.y * 0.19 - time) * 0.78 + sin(position.y * 0.43 + time * 2.0) * 0.24;
    float warpedX      = position.x + verticalWarp;
    float broadPhase   = warpedX * 0.58 + time;
    float narrowPhase  = warpedX * 1.07 - time * 2.0 + sin(position.y * 0.13 + time) * 0.42;
    float broadRibbon  = 0.5 + 0.5 * sin(broadPhase);
    float narrowRibbon = 0.5 + 0.5 * sin(narrowPhase);

    broadRibbon *= broadRibbon;
    narrowRibbon *= narrowRibbon;

    float verticalLight = 0.72 + 0.28 * sin(position.y * 0.27 - time * 2.0 + sin(warpedX * 0.21));
    float curtain       = clamp((broadRibbon * 0.72 + narrowRibbon * 0.38) * verticalLight, 0.0, 1.0);
    float colorMix      = 0.5 + 0.5 * sin(warpedX * 0.31 - position.y * 0.09 + time * 3.0);
    float height        = broadRibbon * 0.68 + narrowRibbon * 0.31 + verticalLight * 0.1;
    return vec3(height, curtain, colorMix);
}

void main() {
    vec2 position = (gl_FragCoord.xy - glassPosition) / glassSize;
    vec3 surface  = auroraSurface(position);
    vec2 normal   = vec2(dFdx(surface.x), dFdy(surface.x)) * glassSize * 0.75;
    normal /= max(1.0, length(normal));

    vec2 uvStep      = normal.x * dFdx(v_texcoord) + normal.y * dFdy(v_texcoord);
    vec2 displacedUV = clamp(v_texcoord + glassRefraction * uvStep, vec2(0.0), vec2(1.0));
    vec4 color       = texture(tex, displacedUV);

    vec4 palette     = mix(auroraColor1, auroraColor2, surface.z);
    float amount     = clamp(auroraIntensity * surface.y, 0.0, 1.0);
    vec3 linearColor = toLinearRGB(color.rgb / max(color.a, 0.001), auroraTransferFunction);

    linearColor = mix(linearColor, palette.rgb / max(palette.a, 0.001), amount * palette.a * 0.3);
    linearColor += palette.rgb * amount * 0.035;

    const vec2 LIGHT_DIRECTION = vec2(-0.451219, 0.892413);
    float emboss = dot(normal, LIGHT_DIRECTION);
    linearColor *= 1.0 + emboss * glassRoughness * 0.08;

    color = fromLinear(vec4(max(linearColor, vec3(0.0)) * color.a, color.a), auroraTransferFunction);
    fragColor = blurFinish(color, v_texcoord, noise, brightness
#if USE_CM
                           ,
                           sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
