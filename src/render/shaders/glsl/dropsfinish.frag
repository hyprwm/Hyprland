#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

precision         highp float;
in vec2           v_texcoord;
uniform sampler2D tex;

uniform float     noise;
uniform float     brightness;
uniform float     glassRefraction;
uniform float     glassSize;
uniform float     glassRoughness;
uniform float     time;
uniform vec2      dropsPosition;
uniform sampler2D sharpTex;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "glassFinish.glsl"

layout(location = 0) out vec4 fragColor;

const float TAU = 6.28318530718;

struct SDropSurface {
    float height;
    float clarity;
};

SDropSurface combineDropSurfaces(SDropSurface first, SDropSurface second) {
    return SDropSurface(max(first.height, second.height), max(first.clarity, second.clarity));
}

vec4 dropRandom(vec2 cell, float seed) {
    vec2 key = cell + vec2(seed, seed * 1.61803);
    return vec4(hash(key + vec2(17.17, 91.73)), hash(key + vec2(63.31, 11.89)), hash(key + vec2(37.61, 53.47)),
                hash(key + vec2(79.13, 41.27)));
}

float sphericalCap(vec2 offset) {
    float distanceSquared = dot(offset, offset);
    float edge = 1.0 - smoothstep(0.82, 1.0, distanceSquared);
    return sqrt(max(0.0, 1.0 - distanceSquared)) * edge;
}

float pulse(float beginValue, float peakValue, float fadeValue, float endValue, float value) {
    return smoothstep(beginValue, peakValue, value) * (1.0 - smoothstep(fadeValue, endValue, value));
}

float smootherStep(float beginValue, float endValue, float value) {
    float progress = clamp((value - beginValue) / max(endValue - beginValue, 0.001), 0.0, 1.0);
    return progress * progress * progress * (progress * (progress * 6.0 - 15.0) + 10.0);
}

float smootherStepVelocity(float beginValue, float endValue, float value) {
    float progress = clamp((value - beginValue) / max(endValue - beginValue, 0.001), 0.0, 1.0);
    float inverseProgress = 1.0 - progress;
    return 30.0 * progress * progress * inverseProgress * inverseProgress / max(endValue - beginValue, 0.001);
}

float trailCenter(float offset, float center, vec3 randomValue) {
    float result = center + sin(offset * mix(7.0, 12.0, randomValue.z)) * mix(0.018, 0.055, randomValue.x);
    return result + sin(offset * 23.0) * 0.009 * randomValue.y;
}

SDropSurface staticRainLayer(vec2 position, float seed, float density) {
    const vec2 CELL_SIZE = vec2(1.45, 3.2);

    float column = floor(position.x / CELL_SIZE.x);
    float columnShift = hash(vec2(column, seed + 9.71));
    vec2 gridPosition = position / CELL_SIZE + vec2(0.0, columnShift);
    vec2 cell = floor(gridPosition);
    vec2 local = fract(gridPosition);
    vec4 randomValue = dropRandom(cell, seed);

    float presence = step(1.0 - density, randomValue.w);
    if (presence <= 0.0)
        return SDropSurface(0.0, 0.0);
    vec2 center = vec2(mix(0.22, 0.78, randomValue.x), mix(0.18, 0.38, randomValue.y));
    vec2 radius = vec2(mix(0.11, 0.17, randomValue.z), mix(0.075, 0.12, randomValue.x));

    vec2 bodyOffset = local - center;
    float verticalPosition = bodyOffset.y / radius.y;
    float taper = mix(1.05, 0.58, smoothstep(-0.75, 0.95, verticalPosition));
    float body = sphericalCap(vec2(bodyOffset.x / (radius.x * taper), verticalPosition));

    float trailStart = center.y + radius.y * 0.55;
    float trailLength = min(mix(0.3, 0.52, randomValue.y), 0.96 - trailStart);
    float trailProgress = (local.y - trailStart) / max(trailLength, 0.001);
    float trailWindow = smoothstep(0.0, 0.08, trailProgress) * (1.0 - smoothstep(0.82, 1.0, trailProgress));

    float trailOffset = max(0.0, local.y - trailStart);
    float trailX = trailCenter(trailOffset, center.x, randomValue.xyz);

    float trailWidth = mix(radius.x * 0.3, radius.x * 0.12, clamp(trailProgress, 0.0, 1.0));
    float trailDistance = abs(local.x - trailX) / max(trailWidth, 0.001);
    float trailProfile = sqrt(max(0.0, 1.0 - trailDistance * trailDistance));
    float breakup = mix(0.45, 1.0, smoothstep(-0.3, 0.25, sin((local.y + randomValue.z) * 43.0)));
    float trail = trailProfile * trailWindow * breakup * 0.24;

    float satelliteProgress = mix(0.28, 0.7, randomValue.z);
    float satelliteY = trailStart + trailLength * satelliteProgress;
    float satelliteOffset = satelliteY - trailStart;
    float satelliteX = trailCenter(satelliteOffset, center.x, randomValue.xyz);
    vec2 satelliteRadius = radius * mix(0.18, 0.3, randomValue.y);
    float satellite = sphericalCap((local - vec2(satelliteX, satelliteY)) / satelliteRadius) * 0.55;

    float height = max(body, max(trail, satellite));
    float clarity = max(smoothstep(0.04, 0.34, body) * 0.72, max(smoothstep(0.015, 0.2, trail) * 0.36, smoothstep(0.04, 0.35, satellite) * 0.5));
    return SDropSurface(height, clarity);
}

float stickSlipProgress(float phase, vec4 randomValue) {
    float shift = (randomValue.z - 0.5) * 0.05;
    float progress = 0.0;
    progress += smootherStep(0.13 + shift, 0.28 + shift, phase) * 0.16;
    progress += smootherStep(0.41 - shift, 0.58 - shift, phase) * 0.29;
    progress += smootherStep(0.68 + shift, 0.91 + shift, phase) * 0.55;
    return progress;
}

float slideAmount(float phase, vec4 randomValue) {
    float shift = (randomValue.z - 0.5) * 0.05;
    float velocity = smootherStepVelocity(0.13 + shift, 0.28 + shift, phase) * 0.16;
    velocity += smootherStepVelocity(0.41 - shift, 0.58 - shift, phase) * 0.29;
    velocity += smootherStepVelocity(0.68 + shift, 0.91 + shift, phase) * 0.55;
    return clamp(velocity / 4.6, 0.0, 1.0);
}

float dropPath(float progress, float start, vec4 randomValue) {
    float phase = randomValue.w * TAU;
    float wave = sin(progress * mix(4.0, 7.0, randomValue.z) + phase) - sin(phase);
    float drift = (randomValue.y - 0.5) * 0.12 * progress;
    return clamp(start + drift + wave * mix(0.012, 0.035, randomValue.x), 0.2, 0.8);
}

SDropSurface movingDrop(vec2 local, vec2 cell, float seed, float density, float animationTime, float cycleRate) {
    vec4 randomValue = dropRandom(cell, seed);

    float presence = step(1.0 - density, randomValue.w);
    if (presence <= 0.0)
        return SDropSurface(0.0, 0.0);
    float phaseOffset = hash(cell + vec2(seed + 43.17, 71.53));
    float rate = (0.75 + floor(hash(cell + vec2(seed + 81.31, 19.47)) * 3.0) * 0.25) * cycleRate;
    float phase = fract(animationTime * rate + phaseOffset);
    float progress = stickSlipProgress(phase, randomValue);
    float sliding = slideAmount(phase, randomValue);

    float startX = mix(0.23, 0.77, randomValue.x);
    float startY = mix(0.45, 0.82, randomValue.y);
    float travelDistance = mix(0.42, 0.82, randomValue.z);
    vec2 center = vec2(dropPath(progress, startX, randomValue), startY - travelDistance * progress);

    float landing = smootherStep(0.0, 0.035, phase);
    float impact = pulse(0.0, 0.014, 0.035, 0.075, phase);
    float drain = smootherStep(0.9, 0.975, phase);
    float bodyLife = landing * (1.0 - smootherStep(0.955, 0.985, phase));
    float scale = mix(0.58, 1.0, landing) * (1.0 + impact * 0.12) * mix(1.0, 0.28, drain);

    vec2 baseRadius = vec2(mix(0.11, 0.17, randomValue.z), mix(0.075, 0.12, randomValue.x));
    vec2 radius = baseRadius * vec2(mix(1.0, 0.82, sliding), mix(1.0, 1.24, sliding)) * scale;
    vec2 bodyOffset = local - center;
    float verticalPosition = bodyOffset.y / radius.y;
    float taper = mix(1.05, 0.58, smoothstep(-0.75, 0.95, verticalPosition));
    float body = sphericalCap(vec2(bodyOffset.x / (radius.x * taper), verticalPosition)) * bodyLife * mix(1.0, 0.42, drain);

    float historicalProgress = (startY - local.y) / travelDistance;
    float trailWindow = smoothstep(-0.015, 0.015, historicalProgress) *
        (1.0 - smoothstep(max(0.0, progress - 0.015), progress + 0.015, historicalProgress));
    float trailProgress = clamp(historicalProgress, 0.0, progress);
    float trailX = dropPath(trailProgress, startX, randomValue);
    float trailWidth = mix(baseRadius.x * 0.13, baseRadius.x * 0.27, clamp(trailProgress / max(progress, 0.001), 0.0, 1.0));
    float trailDistance = abs(local.x - trailX) / max(trailWidth, 0.001);
    float trailProfile = sqrt(max(0.0, 1.0 - trailDistance * trailDistance));
    float trailAge = mix(0.35, 1.0, clamp(trailProgress / max(progress, 0.001), 0.0, 1.0));
    float breakup = mix(0.42, 1.0, smoothstep(-0.35, 0.2, sin((local.y + randomValue.z) * 47.0)));
    float trailLife = landing * (1.0 - smootherStep(0.965, 1.0, phase)) * smootherStep(0.025, 0.12, progress);
    float trail = trailProfile * trailWindow * trailAge * breakup * trailLife * 0.24;

    float satelliteProgress = progress * mix(0.28, 0.62, randomValue.z);
    vec2 satelliteCenter = vec2(dropPath(satelliteProgress, startX, randomValue), startY - travelDistance * satelliteProgress);
    vec2 satelliteRadius = baseRadius * mix(0.18, 0.29, randomValue.y);
    float satellite = sphericalCap((local - satelliteCenter) / satelliteRadius) * trailLife * 0.5;

    float height = max(body, max(trail, satellite));
    float clarity = max(smoothstep(0.04, 0.34, body) * 0.76, max(smoothstep(0.015, 0.2, trail) * 0.42, smoothstep(0.04, 0.35, satellite) * 0.54));
    return SDropSurface(height, clarity);
}

SDropSurface movingRainLayer(vec2 position, float seed, float density, float animationTime, float cycleRate) {
    const vec2 CELL_SIZE = vec2(1.45, 3.2);

    float column = floor(position.x / CELL_SIZE.x);
    float columnShift = hash(vec2(column, seed + 9.71));
    vec2 gridPosition = position / CELL_SIZE + vec2(0.0, columnShift);
    vec2 cell = floor(gridPosition);
    vec2 local = fract(gridPosition);

    SDropSurface drop = movingDrop(local, cell, seed, density, animationTime, cycleRate);
    SDropSurface dropFromAbove = SDropSurface(0.0, 0.0);
    if (local.y > 0.4)
        dropFromAbove = movingDrop(local - vec2(0.0, 1.0), cell + vec2(0.0, 1.0), seed, density, animationTime, cycleRate);
    return combineDropSurfaces(drop, dropFromAbove);
}

SDropSurface rainLayer(vec2 position, float seed, float density, float animationTime, float cycleRate) {
    if (animationTime <= 0.0)
        return staticRainLayer(position, seed, density);
    return movingRainLayer(position, seed, density, animationTime, cycleRate);
}

SDropSurface beadLayer(vec2 position, float seed, float density, float animationTime) {
    vec2 cell = floor(position);
    vec2 local = fract(position);
    vec4 randomValue = dropRandom(cell, seed);

    float presence = step(1.0 - density, randomValue.w);
    if (presence <= 0.0)
        return SDropSurface(0.0, 0.0);
    vec2 center = mix(vec2(0.2), vec2(0.8), randomValue.xy);
    float radius = mix(0.08, 0.19, randomValue.z * randomValue.z);
    vec2 radii = vec2(radius * mix(0.82, 1.08, randomValue.x), radius * mix(0.9, 1.22, randomValue.y));
    float amplitude = mix(0.42, 0.78, randomValue.z);

    if (animationTime <= 0.0) {
        float height = sphericalCap((local - center) / radii) * amplitude;
        return SDropSurface(height, smoothstep(0.04, 0.36, height) * 0.46);
    }

    float behavior = hash(cell + vec2(seed + 57.91, 13.37));
    float persistent = step(0.48, behavior);
    float phaseOffset = hash(cell + vec2(seed + 23.73, 89.11));
    float rate = 0.75 + floor(hash(cell + vec2(seed + 67.19, 31.43)) * 3.0) * 0.25;
    float phase = fract(animationTime * rate + phaseOffset);

    float landing = smootherStep(0.0, 0.03, phase);
    float impact = pulse(0.0, 0.012, 0.03, 0.07, phase) * (1.0 - persistent);
    float transientLife = landing * (1.0 - smootherStep(0.84, 1.0, phase));
    float life = mix(transientLife, 1.0, persistent);
    float transientRadiusScale = mix(0.48, 1.0, landing) * (1.0 + impact * 0.16);
    float edgeDistance = min(min(center.x, 1.0 - center.x), min(center.y, 1.0 - center.y));
    float maximumRadius = max(radii.x, radii.y);
    float persistentRadiusScale = min(1.0, edgeDistance / max(maximumRadius, 0.001));
    transientRadiusScale = min(transientRadiusScale, edgeDistance / max(maximumRadius * 1.7, 0.001));
    float radiusScale = mix(transientRadiusScale, persistentRadiusScale, persistent);

    vec2 normalizedOffset = (local - center) / (radii * max(radiusScale, 0.16));
    float body = sphericalCap(normalizedOffset) * life;
    float ringDistance = abs(length(normalizedOffset) - 1.4);
    float ring = (1.0 - smoothstep(0.1, 0.26, ringDistance)) * impact * 0.08;

    float height = max(body, ring) * amplitude;
    float clarity = max(smoothstep(0.04, 0.36, body * amplitude) * 0.48, smoothstep(0.01, 0.06, ring * amplitude) * 0.16);
    return SDropSurface(height, clarity);
}

SDropSurface dropsSurface(vec2 position) {
    SDropSurface largeDrops = rainLayer(position, 3.17, 0.55, time, 0.45);
    SDropSurface smallDrops = rainLayer(position * 1.43 + vec2(0.37, 1.91), 17.83, 0.35, time, 1.0);
    SDropSurface beads = beadLayer(position * 1.75 + vec2(4.13, 2.71), 31.41, 0.36, time);

    smallDrops.height *= 0.78;
    smallDrops.clarity *= 0.74;
    return combineDropSurfaces(largeDrops, combineDropSurfaces(smallDrops, beads));
}

vec4 dropsFinish(vec2 normal, float clarity) {
    normal /= max(1.0, length(normal));

    vec2 uvStep = normal.x * dFdx(v_texcoord) + normal.y * dFdy(v_texcoord);
    vec2 displacedUV = clamp(v_texcoord + glassRefraction * uvStep, vec2(0.0), vec2(1.0));
    vec4 pixColor = mix(texture(tex, displacedUV), texture(sharpTex, displacedUV), clarity);

    const vec2 LIGHT_DIRECTION = vec2(-0.451219, 0.892413);
    float emboss = dot(normal, LIGHT_DIRECTION);
    pixColor.rgb *= 1.0 + emboss * glassRoughness * 0.12;

    return blurFinish(pixColor, v_texcoord, noise, brightness
#if USE_CM
                      ,
                      sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}

void main() {
    vec2 fragmentPosition = gl_FragCoord.xy - dropsPosition;
    vec2 position = vec2(fragmentPosition.x, -fragmentPosition.y) / glassSize;
    SDropSurface surface = dropsSurface(position);
    vec2 gradient;
    if (time <= 0.0)
        gradient = vec2(dFdx(surface.height), dFdy(surface.height));
    else {
        float pixelStep = 1.0 / glassSize;
        float horizontalHeight = dropsSurface(position + vec2(pixelStep, 0.0)).height;
        float verticalHeight = dropsSurface(position - vec2(0.0, pixelStep)).height;
        gradient = vec2(horizontalHeight - surface.height, verticalHeight - surface.height);
    }
    gradient *= glassSize * 0.16;
    fragColor = dropsFinish(gradient, surface.clarity);
}
