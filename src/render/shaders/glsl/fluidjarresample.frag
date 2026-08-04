#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
precision highp int;

in vec2 v_texcoord;
uniform sampler2D fluidJarParticleTex;
uniform vec2 fluidJarResolution;
uniform vec2 fluidJarGridSize;
uniform int fluidJarParticleCount;
uniform vec2 fluidJarOldGridSize;
uniform int fluidJarOldParticleCount;
uniform vec4 fluidJarTransform;
uniform vec2 fluidJarVelocityScale;
uniform vec4 fluidJarWallVelocities;
uniform float fluidJarMass;

#include "fluidJar.glsl"

layout(location = 0) out vec4 fragColor;

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 gridSize = ivec2(fluidJarGridSize);
    ivec2 storageSize = FLUID_JAR_STATE_SLOT_SIZE * gridSize;
    if (any(greaterThanEqual(pixel, storageSize))) {
        fragColor = vec4(-1.0);
        return;
    }

    ivec2 cell = pixel / FLUID_JAR_STATE_SLOT_SIZE;
    int id = cell.x + cell.y * gridSize.x;
    int field = pixel.x % FLUID_JAR_STATE_SLOT_SIZE.x;
    if (id >= fluidJarParticleCount || id >= fluidJarOldParticleCount) {
        fragColor = vec4(-1.0);
        return;
    }

    FluidJarParticle particle = fluidJarLoadParticle(fluidJarParticleTex, id, ivec2(fluidJarOldGridSize));
    particle.position = particle.position * fluidJarTransform.xy + fluidJarTransform.zw;
    particle.velocity *= fluidJarVelocityScale;
    if (any(greaterThan(abs(fluidJarTransform.xy - vec2(1.0)), vec2(0.001)))) {
        particle.pressure = 0.0;
        particle.divergence = 0.0;
    }
    fluidJarResolveBoundaries(particle, fluidJarResolution, fluidJarWallVelocities, fluidJarMass);
    fragColor = fluidJarSaveParticle(particle, field);
}
