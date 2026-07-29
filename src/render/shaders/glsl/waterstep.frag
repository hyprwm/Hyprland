#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord;

uniform sampler2D waterStateTex;
uniform vec2 waterTexelSize;
uniform vec4 waterParams;

const int MAX_WATER_IMPULSES = 16;
uniform int waterImpulseCount;
uniform vec4 waterImpulses[MAX_WATER_IMPULSES];

layout(location = 0) out vec4 fragColor;

float decodeState(float value) {
    return value * 2.0 - 1.0;
}

float encodeState(float value) {
    return clamp(value * 0.5 + 0.5, 0.0, 1.0);
}

void main() {
    vec2 state = texture(waterStateTex, v_texcoord).rg;
    float height = decodeState(state.r);
    float velocity = decodeState(state.g);

    float left = decodeState(texture(waterStateTex, v_texcoord - vec2(waterTexelSize.x, 0.0)).r);
    float right = decodeState(texture(waterStateTex, v_texcoord + vec2(waterTexelSize.x, 0.0)).r);
    float up = decodeState(texture(waterStateTex, v_texcoord - vec2(0.0, waterTexelSize.y)).r);
    float down = decodeState(texture(waterStateTex, v_texcoord + vec2(0.0, waterTexelSize.y)).r);

    float frameStep = clamp(waterParams.x * 60.0, 0.0, 3.0);
    float propagation = waterParams.y * 0.24;
    float damping = pow(waterParams.z, frameStep);
    float laplacian = left + right + up + down - 4.0 * height;

    velocity = (velocity + laplacian * propagation * frameStep) * damping;
    height += velocity * frameStep;

    float edgeDistance = min(min(v_texcoord.x, 1.0 - v_texcoord.x), min(v_texcoord.y, 1.0 - v_texcoord.y));
    float edgeFade = smoothstep(0.0, 0.04, edgeDistance);
    velocity *= edgeFade;
    height *= mix(0.9, 1.0, edgeFade);

    for (int i = 0; i < MAX_WATER_IMPULSES; ++i) {
        if (i >= waterImpulseCount)
            break;

        vec2 delta = v_texcoord - waterImpulses[i].xy;
        float radius = max(waterImpulses[i].z, 0.0001);
        float influence = exp(-dot(delta, delta) / (radius * radius));
        height += influence * waterImpulses[i].w;
    }

    fragColor = vec4(encodeState(height), encodeState(velocity), 0.0, 1.0);
}
