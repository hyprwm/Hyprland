#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision highp float;
precision highp int;
precision highp usampler2D;

in vec2 v_texcoord;
uniform sampler2D fluidJarParticleTex;
uniform usampler2D fluidJarGraphTex;
uniform usampler2D fluidJarTrackingTex;
uniform vec2 fluidJarResolution;
uniform vec2 fluidJarGridSize;
uniform int fluidJarParticleCount;
uniform int fluidJarFrame;

#include "fluidJar.glsl"

layout(location = 0) out uvec4 fragColor;

float nearestDistance = 1e10;
int nearestId = -1;
vec2 position;

void sortCandidate(int candidate, ivec2 gridSize) {
    if (candidate < 0 || candidate >= fluidJarParticleCount)
        return;
    vec2 particlePosition = texelFetch(fluidJarParticleTex, fluidJarAddress(candidate, 0, gridSize), 0).xy;
    float candidateDistance = distance(position, particlePosition);
    if (candidateDistance >= nearestDistance)
        return;
    nearestDistance = candidateDistance;
    nearestId = candidate;
}

void sortGridCandidate(int row, ivec2 gridSize) {
    if (row < 0 || row >= gridSize.y)
        return;

    int rowStart = row * gridSize.x;
    int rowCount = min(gridSize.x, fluidJarParticleCount - rowStart);
    if (rowCount <= 0)
        return;

    vec2 spacing = fluidJarResolution / vec2(gridSize);
    float centering = 0.5 * float(gridSize.x - rowCount);
    int column = clamp(int(position.x / spacing.x - centering), 0, rowCount - 1);
    sortCandidate(rowStart + column, gridSize);
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 gridSize = ivec2(fluidJarGridSize);
    position = vec2(pixel) + vec2(0.5);

    int previousId = fluidJarDecodeId(texelFetch(fluidJarTrackingTex, clamp(pixel, ivec2(0), ivec2(fluidJarResolution) - 1), 0).rg);
    sortCandidate(previousId, gridSize);

    int estimatedRow = min(int(position.y / (fluidJarResolution.y / float(gridSize.y))), (fluidJarParticleCount - 1) / gridSize.x);
    sortGridCandidate(estimatedRow - 1, gridSize);
    sortGridCandidate(estimatedRow, gridSize);
    sortGridCandidate(estimatedRow + 1, gridSize);

    for (int index = 0; index < 8; ++index) {
        ivec2 samplePixel = clamp(pixel + fluidJarCrossDistribution(index), ivec2(0), ivec2(fluidJarResolution) - 1);
        sortCandidate(fluidJarDecodeId(texelFetch(fluidJarTrackingTex, samplePixel, 0).rg), gridSize);
    }

    for (int index = 0; index < 5; ++index)
        sortCandidate(int(float(fluidJarParticleCount) *
                          fluidJarHash13(vec3(float(fluidJarFrame + index) + 0.5, float(pixel.x) + 0.37, float(pixel.y) + 0.61))),
                      gridSize);

    if (nearestId >= 0) {
        for (int direction = 0; direction < 4; ++direction) {
            ivec4 neighbors = fluidJarLoadNeighbors(fluidJarGraphTex, nearestId, direction, gridSize);
            for (int index = 0; index < 4; ++index)
                sortCandidate(neighbors[index], gridSize);
        }
    }

    fragColor = uvec4(fluidJarEncodeId(nearestId), 0u, 0u);
}
