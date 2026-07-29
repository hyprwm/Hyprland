#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif

precision highp usampler2D;

const float FLUID_JAR_PI = 3.14159265;
const ivec2 FLUID_JAR_STATE_SLOT_SIZE = ivec2(4, 1);
const ivec2 FLUID_JAR_GRAPH_SLOT_SIZE = ivec2(8, 1);

struct FluidJarParticle {
    int id;
    vec2 position;
    vec2 velocity;
    float pressure;
    float density;
    float smoothScale;
    float divergence;
    vec4 data;
};

ivec2 fluidJarAddress(int id, int field, ivec2 gridSize) {
    return FLUID_JAR_STATE_SLOT_SIZE * ivec2(id % gridSize.x, id / gridSize.x) + ivec2(field, 0);
}

ivec2 fluidJarGraphAddress(int id, int direction, int pairIndex, ivec2 gridSize) {
    return FLUID_JAR_GRAPH_SLOT_SIZE * ivec2(id % gridSize.x, id / gridSize.x) + ivec2(direction * 2 + pairIndex, 0);
}

uvec2 fluidJarEncodeId(int id) {
    if (id < 0)
        return uvec2(0u);

    uint value = uint(id) + 1u;
    return uvec2(value & 65535u, value >> 16u);
}

int fluidJarDecodeId(uvec2 encoded) {
    uint value = encoded.x | (encoded.y << 16u);
    return value == 0u ? -1 : int(value - 1u);
}

uvec4 fluidJarEncodeIdPair(ivec2 ids) {
    return uvec4(fluidJarEncodeId(ids.x), fluidJarEncodeId(ids.y));
}

ivec2 fluidJarDecodeIdPair(uvec4 encoded) {
    return ivec2(fluidJarDecodeId(encoded.rg), fluidJarDecodeId(encoded.ba));
}

ivec4 fluidJarLoadNeighbors(usampler2D graphTex, int id, int direction, ivec2 gridSize) {
    ivec2 first = fluidJarDecodeIdPair(texelFetch(graphTex, fluidJarGraphAddress(id, direction, 0, gridSize), 0));
    ivec2 second = fluidJarDecodeIdPair(texelFetch(graphTex, fluidJarGraphAddress(id, direction, 1, gridSize), 0));
    return ivec4(first, second);
}

FluidJarParticle fluidJarLoadParticle(sampler2D particleTex, int id, ivec2 gridSize) {
    FluidJarParticle particle;
    vec4 value = texelFetch(particleTex, fluidJarAddress(id, 0, gridSize), 0);
    particle.position = value.xy;
    particle.velocity = value.zw;

    value = texelFetch(particleTex, fluidJarAddress(id, 1, gridSize), 0);
    particle.pressure = value.x;
    particle.density = value.y;
    particle.smoothScale = value.z;
    particle.divergence = value.w;
    particle.data = texelFetch(particleTex, fluidJarAddress(id, 2, gridSize), 0);
    particle.id = id;
    return particle;
}

vec4 fluidJarSaveParticle(FluidJarParticle particle, int field) {
    if (field == 0)
        return vec4(particle.position, particle.velocity);
    if (field == 1)
        return vec4(particle.pressure, particle.density, particle.smoothScale, particle.divergence);
    if (field == 2)
        return particle.data;
    return vec4(0.0);
}

void fluidJarResolveBoundaries(inout FluidJarParticle particle, vec2 resolution, vec3 wallVelocities, float mass) {
    float restitution = clamp(0.12 / sqrt(max(mass, 0.1)), 0.02, 0.4);
    float left = min(2.0, resolution.x * 0.5);
    float right = max(resolution.x - 2.0, left);
    float bottom = min(2.0, resolution.y);

    if (particle.position.x < left) {
        particle.position.x = left;
        if (particle.velocity.x < wallVelocities.x)
            particle.velocity.x = wallVelocities.x - restitution * (particle.velocity.x - wallVelocities.x);
    }

    if (particle.position.x > right) {
        particle.position.x = right;
        if (particle.velocity.x > wallVelocities.y)
            particle.velocity.x = wallVelocities.y - restitution * (particle.velocity.x - wallVelocities.y);
    }

    if (particle.position.y < bottom) {
        particle.position.y = bottom;
        if (particle.velocity.y < wallVelocities.z)
            particle.velocity.y = wallVelocities.z - restitution * (particle.velocity.y - wallVelocities.z);
    }
}

float fluidJarSquared(float value) {
    return value * value + 1e-2;
}

float fluidJarKernel(float distanceValue, float scale) {
    return exp(-fluidJarSquared(distanceValue / scale)) / (FLUID_JAR_PI * fluidJarSquared(scale));
}

float fluidJarKernelGradient(float distanceValue, float scale) {
    return 2.0 * distanceValue * fluidJarKernel(distanceValue, scale) / fluidJarSquared(scale);
}

float fluidJarHash13(vec3 value) {
    value = fract(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return fract((value.x + value.y) * value.z);
}

ivec2 fluidJarCrossDistribution(int index) {
    return (1 << (index / 4)) * ivec2(((index & 2) / 2) ^ 1, (index & 2) / 2) * (2 * (index % 2) - 1);
}
