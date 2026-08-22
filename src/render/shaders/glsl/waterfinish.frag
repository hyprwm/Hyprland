#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord;

uniform sampler2D tex;
uniform sampler2D waterStateTex;
uniform int waterEnabled;
uniform vec2 waterTexelSize;
uniform vec4 waterExtent;
uniform float waterRefraction;
uniform float noise;
uniform float brightness;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;

float waterHeight(vec2 uv) {
    return texture(waterStateTex, clamp(uv, vec2(0.0), vec2(1.0))).r * 2.0 - 1.0;
}

void main() {
    vec2 displacedUV = v_texcoord;
    if (waterEnabled != 0) {
        vec2 position = v_texcoord * vec2(textureSize(tex, 0));
        vec2 waterUV = (position - waterExtent.xy) / waterExtent.zw;
        if (all(greaterThanEqual(waterUV, vec2(0.0))) && all(lessThanEqual(waterUV, vec2(1.0)))) {
            float left = waterHeight(waterUV - vec2(waterTexelSize.x, 0.0));
            float right = waterHeight(waterUV + vec2(waterTexelSize.x, 0.0));
            float up = waterHeight(waterUV - vec2(0.0, waterTexelSize.y));
            float down = waterHeight(waterUV + vec2(0.0, waterTexelSize.y));
            vec2 gradient = vec2(left - right, up - down);
            float gradientLength = length(gradient);
            if (gradientLength > 1.0)
                gradient /= gradientLength;
            vec2 texSize = vec2(textureSize(tex, 0));
            displacedUV = clamp(v_texcoord + gradient * waterRefraction / texSize, vec2(0.0), vec2(1.0));
        }
    }

    fragColor = blurFinish(texture(tex, displacedUV), v_texcoord, noise, brightness
#if USE_CM
                           ,
                           sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
