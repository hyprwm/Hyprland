#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
precision highp int;

in vec2 v_texcoord;
uniform vec2 fluidJarResolution;
uniform vec2 fluidJarGridSize;
uniform int fluidJarParticleCount;

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
    if (id >= fluidJarParticleCount) {
        fragColor = vec4(-1.0);
        return;
    }

    FluidJarParticle particle;
    particle.id = id;
    vec2 spacing = fluidJarResolution / vec2(gridSize);
    particle.position = (vec2(cell) + vec2(0.5)) * spacing;
    int partialRowCount = fluidJarParticleCount % gridSize.x;
    if (partialRowCount > 0 && cell.y == fluidJarParticleCount / gridSize.x)
        particle.position.x += 0.5 * float(gridSize.x - partialRowCount) * spacing.x;
    particle.velocity = vec2(0.0);
    particle.pressure = 0.0;
    particle.density = 5.0;
    particle.smoothScale = 1.0;
    particle.divergence = 0.0;
    float materialA = 0.5 + 0.25 * sin(dot(particle.position, vec2(0.17, 0.11))) + 0.25 * sin(dot(particle.position, vec2(-0.09, 0.23)) + 1.7);
    float materialB = 0.5 + 0.25 * sin(dot(particle.position, vec2(-0.13, 0.07)) + 0.8) + 0.25 * sin(dot(particle.position, vec2(0.05, 0.19)) + 2.4);
    float shapeA = fluidJarHash13(vec3(float(id) + 0.37, 1.73, 4.91));
    float shapeB = fluidJarHash13(vec3(float(id) + 2.11, 5.29, 0.83));
    particle.data = vec4(clamp(materialA, 0.0, 1.0), clamp(materialB, 0.0, 1.0), shapeA, shapeB);
    fragColor = fluidJarSaveParticle(particle, field);
}
