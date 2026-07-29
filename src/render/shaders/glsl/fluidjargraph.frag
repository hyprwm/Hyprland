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
uniform int fluidJarFrame;

#include "fluidJar.glsl"

layout(location = 0) out uvec4 fragColor;

ivec4 nearestIds = ivec4(-1);
vec4 nearestDistances = vec4(1e6);

void insertNearest(float distanceValue, int id) {
    if (nearestDistances.x > distanceValue) {
        nearestDistances = vec4(distanceValue, nearestDistances.xyz);
        nearestIds = ivec4(id, nearestIds.xyz);
    } else if (nearestDistances.y > distanceValue) {
        nearestDistances.yzw = vec3(distanceValue, nearestDistances.yz);
        nearestIds.yzw = ivec3(id, nearestIds.yz);
    } else if (nearestDistances.z > distanceValue) {
        nearestDistances.zw = vec2(distanceValue, nearestDistances.z);
        nearestIds.zw = ivec2(id, nearestIds.z);
    } else if (nearestDistances.w > distanceValue) {
        nearestDistances.w = distanceValue;
        nearestIds.w = id;
    }
}

bool alreadySorted(int id, int currentId) {
    return id < 0 || id >= fluidJarParticleCount || id == currentId || any(equal(nearestIds, ivec4(id)));
}

int neighborDirection(vec2 delta, int firstId, int secondId) {
    if (dot(delta, delta) < 1e-6) {
        float pairHash = fluidJarHash13(vec3(float(min(firstId, secondId)), float(max(firstId, secondId)), 0.731));
        int pairDirection = min(int(pairHash * 4.0), 3);
        return firstId < secondId ? pairDirection : pairDirection ^ 1;
    }

    if (abs(delta.x) >= abs(delta.y))
        return delta.x >= 0.0 ? 0 : 1;
    return delta.y >= 0.0 ? 2 : 3;
}

void sortCandidate(int candidate, int direction, FluidJarParticle particle, ivec2 gridSize) {
    if (alreadySorted(candidate, particle.id))
        return;

    vec2 neighborPosition = texelFetch(fluidJarParticleTex, fluidJarAddress(candidate, 0, gridSize), 0).xy;
    vec2 delta = neighborPosition - particle.position;
    if (neighborDirection(delta, particle.id, candidate) != direction)
        return;
    insertNearest(length(delta), candidate);
}

void sortGridCandidate(int row, int columnOffset, int direction, FluidJarParticle particle, ivec2 gridSize) {
    if (row < 0 || row >= gridSize.y)
        return;

    int rowStart = row * gridSize.x;
    int rowCount = min(gridSize.x, fluidJarParticleCount - rowStart);
    if (rowCount <= 0)
        return;

    vec2 spacing = fluidJarResolution / vec2(gridSize);
    float centering = 0.5 * float(gridSize.x - rowCount);
    int column = clamp(int(particle.position.x / spacing.x - centering) + columnOffset, 0, rowCount - 1);
    sortCandidate(rowStart + column, direction, particle, gridSize);
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 gridSize = ivec2(fluidJarGridSize);
    ivec2 storageSize = FLUID_JAR_GRAPH_SLOT_SIZE * gridSize;
    if (any(greaterThanEqual(pixel, storageSize))) {
        fragColor = uvec4(0u);
        return;
    }

    ivec2 cell = pixel / FLUID_JAR_GRAPH_SLOT_SIZE;
    int id = cell.x + cell.y * gridSize.x;
    int field = pixel.x % FLUID_JAR_GRAPH_SLOT_SIZE.x;
    int direction = field / 2;
    int pairIndex = field % 2;
    if (id >= fluidJarParticleCount) {
        fragColor = uvec4(0u);
        return;
    }

    FluidJarParticle particle = fluidJarLoadParticle(fluidJarParticleTex, id, gridSize);
    int estimatedRow = min(int(particle.position.y / (fluidJarResolution.y / float(gridSize.y))), (fluidJarParticleCount - 1) / gridSize.x);
    for (int offset = -4; offset <= 4; ++offset)
        sortGridCandidate(estimatedRow + offset, 0, direction, particle, gridSize);
    for (int offset = 1; offset <= 2; ++offset) {
        sortGridCandidate(estimatedRow, -offset, direction, particle, gridSize);
        sortGridCandidate(estimatedRow, offset, direction, particle, gridSize);
    }

    for (int index = 0; index < 8; ++index)
        sortCandidate(int(float(fluidJarParticleCount) * fluidJarHash13(vec3(float(fluidJarFrame), float(id), float(index)))), direction, particle, gridSize);

    ivec4 directNeighbors = fluidJarLoadNeighbors(fluidJarGraphTex, id, direction, gridSize);
    for (int index = 0; index < 4; ++index) {
        int neighborId = directNeighbors[index];
        sortCandidate(neighborId, direction, particle, gridSize);
        if (neighborId < 0 || neighborId >= fluidJarParticleCount)
            continue;

        ivec4 indirectNeighbors = fluidJarLoadNeighbors(fluidJarGraphTex, neighborId, (fluidJarFrame + id) % 4, gridSize);
        for (int indirect = 0; indirect < 2; ++indirect)
            sortCandidate(indirectNeighbors[indirect], direction, particle, gridSize);
    }

    fragColor = fluidJarEncodeIdPair(pairIndex == 0 ? nearestIds.xy : nearestIds.zw);
}
