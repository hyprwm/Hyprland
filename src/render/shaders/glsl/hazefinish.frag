#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;
in vec2           v_texcoord;
uniform sampler2D tex;

uniform float     noise;
uniform float     brightness;
uniform float     hazeIntensity;
uniform float     hazeIridescence;
uniform int       hazeTransferFunction;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "cm_helpers.glsl"
#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;

const vec3 BT709_LUMA = vec3(0.2126, 0.7152, 0.0722);
const vec3 PEARL_COOL = vec3(0.55, 1.08, 1.35);
const vec3 PEARL_MID  = vec3(1.28, 0.86, 1.30);
const vec3 PEARL_WARM = vec3(1.40, 0.92, 0.58);

vec3 pearlColor(float phase) {
    float position = phase * 0.5 + 0.5;
    vec3 color;
    if (position < 0.5)
        color = mix(PEARL_COOL, PEARL_MID, position * 2.0);
    else
        color = mix(PEARL_MID, PEARL_WARM, (position - 0.5) * 2.0);
    return color / max(dot(color, BT709_LUMA), 0.001);
}

vec4 applyHaze(vec4 color) {
    float intensity = clamp(hazeIntensity, 0.0, 1.0);
    if (intensity <= 0.0 || color.a <= 0.001)
        return color;

    float alpha        = color.a;
    vec3 linearColor   = toLinearRGB(max(color.rgb / alpha, vec3(0.0)), hazeTransferFunction);
    float luminance    = max(dot(linearColor, BT709_LUMA), 0.0);
    float phase        = clamp(dot(v_texcoord - vec2(0.5), vec2(1.28, -0.72)), -1.0, 1.0);
    float iridescence  = clamp(hazeIridescence, 0.0, 1.0);
    vec3 spectralShift = luminance * (pearlColor(phase) - vec3(1.0)) * iridescence * 0.65;
    float sheen        = 1.0 + (1.0 - abs(phase)) * 0.08;
    vec3 filmColor     = max((linearColor + spectralShift) * sheen, vec3(0.0));

    linearColor = mix(linearColor, filmColor, intensity);
    return fromLinear(vec4(max(linearColor, vec3(0.0)) * alpha, alpha), hazeTransferFunction);
}

void main() {
    vec4 color = applyHaze(texture(tex, v_texcoord));

    fragColor = blurFinish(color, v_texcoord, noise, brightness
#if USE_CM
                           ,
                           sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
