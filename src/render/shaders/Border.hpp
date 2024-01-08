#pragma once

#include <string>

// makes a stencil without corners
inline const std::string FRAGBORDER1 = R"#(
precision mediump float;
varying vec4 v_color;
varying vec2 v_texcoord;

uniform vec2 topLeft;
uniform vec2 fullSize;
uniform vec2 fullSizeUntransformed;
uniform vec4 cornerRadii;
uniform vec4 cornerRadiiOuter;
uniform float thick;

uniform vec4 gradient[10];
uniform int gradientLength;
uniform float angle;
uniform float alpha;

vec4 getColorForCoord(vec2 normalizedCoord) {
    if (gradientLength < 2)
        return gradient[0];

    float finalAng = 0.0;

    if (angle > 4.71 /* 270 deg */) {
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = 6.28 - angle;
    } else if (angle > 3.14 /* 180 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = angle - 3.14;
    } else if (angle > 1.57 /* 90 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        finalAng = 3.14 - angle;
    } else {
        finalAng = angle;
    }

    float sine = sin(finalAng);

    float progress = (normalizedCoord[1] * sine + normalizedCoord[0] * (1.0 - sine)) * float(gradientLength - 1);
    int bottom = int(floor(progress));
    int top = bottom + 1;

    return gradient[top] * (progress - float(bottom)) + gradient[bottom] * (float(top) - progress);
}

void main() {

    highp vec2 pixCoord = vec2(gl_FragCoord);
    highp vec2 pixCoordOuter = pixCoord;
    highp vec2 originalPixCoord = v_texcoord;
    originalPixCoord *= fullSizeUntransformed;
    float additionalAlpha = 1.0;

    vec4 pixColor = vec4(1.0, 1.0, 1.0, 1.0);

    bool done = false;

    pixCoord -= topLeft + fullSize * 0.5;

    bvec2 positive = lessThan(vec2(0.0), pixCoord);

    vec4 pick = vec4(
        !positive.x && !positive.y,
        positive.x && !positive.y,
        positive.x && positive.y,
        !positive.x && positive.y);

    vec4 result = pick * cornerRadii;
    vec4 resultOuter = pick * cornerRadiiOuter;

    float radius = result.x + result.y + result.z + result.w;
    float radiusOuter = resultOuter.x + resultOuter.y + resultOuter.z + resultOuter.w;

    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoordOuter = pixCoord;

    pixCoord -= fullSize * 0.5 - radius;
    pixCoordOuter -= fullSize * 0.5 - radiusOuter;

    // center the pixes dont make it top-left
    pixCoord += vec2(1.0, 1.0) / fullSize;
    pixCoordOuter += vec2(1.0, 1.0) / fullSize;

    if (min(pixCoord.x, pixCoord.y) > 0.0 && radius > 0.0) {
	    float dist = length(pixCoord);
	    float distOuter = length(pixCoordOuter);
        float h = (thick / 2.0);

	    if (dist < radius - h) {
            // lower
            float normalized = smoothstep(0.0, 1.0, dist - radius + thick + 0.5);
            additionalAlpha *= normalized;
            done = true;
        } else if (min(pixCoordOuter.x, pixCoordOuter.y) > 0.0) {
            // higher
            float normalized = 1.0 - smoothstep(0.0, 1.0, distOuter - radiusOuter + 0.5);
            additionalAlpha *= normalized;
            done = true;
        } else if (distOuter < radiusOuter - h) {
            additionalAlpha = 1.0;
            done = true;
        }
    }

    // now check for other shit
    if (!done) {
        // distance to all straight bb borders
        float distanceT = originalPixCoord[1];
        float distanceB = fullSizeUntransformed[1] - originalPixCoord[1];
        float distanceL = originalPixCoord[0];
        float distanceR = fullSizeUntransformed[0] - originalPixCoord[0];

        // get the smallest
        float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));

        if (smallest > thick)
            discard;
    }

    if (additionalAlpha == 0.0)
        discard;

    pixColor = getColorForCoord(v_texcoord);
    pixColor.rgb *= pixColor[3];

    pixColor *= alpha * additionalAlpha;

    gl_FragColor = pixColor;
}
)#";
