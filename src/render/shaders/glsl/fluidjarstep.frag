#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
precision highp int;
precision highp usampler2D;

in vec2 v_texcoord;
uniform sampler2D fluidJarParticleTex;
uniform usampler2D fluidJarGraphTex;
uniform vec2 fluidJarResolution;
uniform vec2 fluidJarGridSize;
uniform int fluidJarParticleCount;
uniform float fluidJarDt;
uniform vec3 fluidJarWallVelocities;
uniform float fluidJarMass;

#include "fluidJar.glsl"

layout(location = 0) out vec4 fragColor;

bool isReciprocalNeighbor(int particleId, int neighborId, int direction, ivec2 gridSize) {
    return any(equal(fluidJarLoadNeighbors(fluidJarGraphTex, neighborId, direction ^ 1, gridSize), ivec4(particleId)));
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 gridSize = ivec2(fluidJarGridSize);
    ivec2 storageSize = FLUID_JAR_STATE_SLOT_SIZE * gridSize;
    if (any(greaterThanEqual(pixel, storageSize))) {
        fragColor = vec4(0.0);
        return;
    }

    ivec2 cell = pixel / FLUID_JAR_STATE_SLOT_SIZE;
    int id = cell.x + cell.y * gridSize.x;
    int field = pixel.x % FLUID_JAR_STATE_SLOT_SIZE.x;
    if (id >= fluidJarParticleCount) {
        fragColor = vec4(0.0);
        return;
    }

    FluidJarParticle particle = fluidJarLoadParticle(fluidJarParticleTex, id, gridSize);
    vec2 force = vec2(0.0, -0.001);
    float scale = 0.21 / 0.036;
    float divergence = 0.0;
    float density = fluidJarKernel(0.0, scale);
    vec2 averageMaterial = particle.data.xy;
    float neighborCount = 1.0;

    for (int direction = 0; direction < 4; ++direction) {
        ivec4 neighbors = fluidJarLoadNeighbors(fluidJarGraphTex, id, direction, gridSize);
        for (int index = 0; index < 4; ++index) {
            int neighborId = neighbors[index];
            if (neighborId < 0 || neighborId >= fluidJarParticleCount)
                continue;
            if (!isReciprocalNeighbor(id, neighborId, direction, gridSize))
                continue;

            FluidJarParticle neighbor = fluidJarLoadParticle(fluidJarParticleTex, neighborId, gridSize);
            float distanceValue = distance(particle.position, neighbor.position);
            vec2 velocityDelta = neighbor.velocity - particle.velocity;
            vec2 positionDelta = neighbor.position - particle.position;
            vec2 directionVector = positionDelta / (distanceValue + 0.001);
            float kernel = fluidJarKernel(distanceValue, scale);
            float velocityProjection = dot(directionVector, velocityDelta);
            vec2 pressureForce = -(neighbor.pressure / fluidJarSquared(neighbor.density) + particle.pressure / fluidJarSquared(particle.density)) * directionVector * kernel;
            divergence += velocityProjection * kernel;
            density += kernel;
            averageMaterial += neighbor.data.xy;
            vec2 viscosity = 1.4 * (3.0 + 3.0 * length(velocityDelta)) * directionVector * velocityProjection * kernel;
            force += pressureForce / fluidJarMass + viscosity;
            neighborCount += 1.0;
        }
    }

    particle.density = density;
    particle.divergence = divergence;
    particle.smoothScale = 0.0;
    float waterPressure = 0.035 * 0.036 * (pow(abs(particle.density / 0.036), 7.0) - 1.0);
    particle.pressure = clamp(waterPressure, 0.0, 0.04);
    particle.velocity += force * fluidJarDt;
    particle.velocity -= particle.velocity * (0.5 * tanh(8.0 * (length(particle.velocity) - 1.5)) + 0.5);
    particle.position += particle.velocity * fluidJarDt;
    fluidJarResolveBoundaries(particle, fluidJarResolution, fluidJarWallVelocities, fluidJarMass);
    particle.data.xy = mix(particle.data.xy, averageMaterial / neighborCount, 0.003);
    fragColor = fluidJarSaveParticle(particle, field);
}
