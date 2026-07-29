#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;
in vec2           v_texcoord;
uniform sampler2D tex;

uniform float     noise;
uniform float     brightness;

const int         MAX_RIPPLE_IMPULSES = 256;
uniform int       rippleCount;
uniform vec4      rippleImpulses[MAX_RIPPLE_IMPULSES];
uniform vec4      rippleParams;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;

void main() {
    vec2 texSize = vec2(textureSize(tex, 0));
    vec2 position = v_texcoord * texSize;
    vec2 displacement = vec2(0.0);

    float duration = max(rippleParams.x, 0.001);
    float maximumRadius = max(rippleParams.y, 1.0);
    float waveWidth = max(rippleParams.z, 1.0);
    float amplitude = max(rippleParams.w, 0.0);

    for (int i = 0; i < MAX_RIPPLE_IMPULSES; ++i) {
        if (i >= rippleCount)
            break;

        float progress = clamp(rippleImpulses[i].z / duration, 0.0, 1.0);
        vec2 delta = position - rippleImpulses[i].xy;
        float distance = length(delta);
        vec2 direction = delta / max(distance, 1.0);

        float waveRadius = maximumRadius * progress;
        float signedDistance = distance - waveRadius;
        float distanceFromWave = abs(signedDistance);
        if (distanceFromWave >= waveWidth)
            continue;

        float envelope = 0.5 + 0.5 * cos(3.14159265359 * distanceFromWave / waveWidth);
        float wave = cos(6.28318530718 * signedDistance / waveWidth);
        displacement += direction * wave * envelope * (1.0 - progress);
    }

    float displacementLength = length(displacement);
    if (displacementLength > 1.0)
        displacement /= displacementLength;
    displacement *= amplitude;

    vec2 displacedUV = clamp(v_texcoord + displacement / texSize, vec2(0.0), vec2(1.0));
    vec4 pixColor = texture(tex, displacedUV);

    fragColor = blurFinish(pixColor, v_texcoord, noise, brightness
#if USE_CM
                           ,
                           sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
