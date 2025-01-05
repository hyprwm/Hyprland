#pragma once

#include <string>
#include <format>
#include "SharedValues.hpp"

// makes a stencil without corners
inline const std::string FRAGBORDER1 = R"#(
precision highp float;
varying vec4 v_color;
varying vec2 v_texcoord;

uniform vec2 topLeft;
uniform vec2 fullSize;
uniform vec2 fullSizeUntransformed;
uniform float radius;
uniform float radiusOuter;
uniform float roundingPower;
uniform float thick;

// Gradients are in OkLabA!!!! {l, a, b, alpha}
uniform vec4 gradient[10];
uniform vec4 gradient2[10];
uniform int gradientLength;
uniform int gradient2Length;
uniform float angle;
uniform float angle2;
uniform float gradientLerp;
uniform float alpha;

float linearToGamma(float x) {
    return x >= 0.0031308 ? 1.055 * pow(x, 0.416666666) - 0.055 : 12.92 * x;
}

vec4 okLabAToSrgb(vec4 lab) {
    float l = pow(lab[0] + lab[1] * 0.3963377774 + lab[2] * 0.2158037573, 3.0);
    float m = pow(lab[0] + lab[1] * (-0.1055613458) + lab[2] * (-0.0638541728), 3.0);
    float s = pow(lab[0] + lab[1] * (-0.0894841775) + lab[2] * (-1.2914855480), 3.0);

    return vec4(linearToGamma(l * 4.0767416621 + m * -3.3077115913 + s * 0.2309699292), 
                linearToGamma(l * (-1.2684380046) + m * 2.6097574011 + s * (-0.3413193965)),
                linearToGamma(l * (-0.0041960863) + m * (-0.7034186147) + s * 1.7076147010),
                lab[3]);
}

vec4 getOkColorForCoordArray1(vec2 normalizedCoord) {
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

vec4 getOkColorForCoordArray2(vec2 normalizedCoord) {
    if (gradient2Length < 2)
        return gradient2[0];

    float finalAng = 0.0;

    if (angle2 > 4.71 /* 270 deg */) {
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = 6.28 - angle;
    } else if (angle2 > 3.14 /* 180 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = angle - 3.14;
    } else if (angle2 > 1.57 /* 90 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        finalAng = 3.14 - angle2;
    } else {
        finalAng = angle2;
    }

    float sine = sin(finalAng);

    float progress = (normalizedCoord[1] * sine + normalizedCoord[0] * (1.0 - sine)) * float(gradient2Length - 1);
    int bottom = int(floor(progress));
    int top = bottom + 1;

    return gradient2[top] * (progress - float(bottom)) + gradient2[bottom] * (float(top) - progress);
}

vec4 getColorForCoord(vec2 normalizedCoord) {
    vec4 result1 = getOkColorForCoordArray1(normalizedCoord);

    if (gradient2Length <= 0)
        return okLabAToSrgb(result1);

    vec4 result2 = getOkColorForCoordArray2(normalizedCoord);

    return okLabAToSrgb(mix(result1, result2, gradientLerp));
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
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoordOuter = pixCoord;
    pixCoord -= fullSize * 0.5 - radius;
    pixCoordOuter -= fullSize * 0.5 - radiusOuter;

    // center the pixes dont make it top-left
    pixCoord += vec2(1.0, 1.0) / fullSize;
    pixCoordOuter += vec2(1.0, 1.0) / fullSize;

    if (min(pixCoord.x, pixCoord.y) > 0.0 && radius > 0.0) {
        // smoothing constant for the edge: more = blurrier, but smoother
        const float SMOOTHING_CONSTANT = )#" +
    std::format("{:.7f}", SHADER_ROUNDED_SMOOTHING_FACTOR) + R"#(;

	    float dist = pow(pow(pixCoord.x,roundingPower)+pow(pixCoord.y,roundingPower),1.0/roundingPower);
	    float distOuter = pow(pow(pixCoordOuter.x,roundingPower)+pow(pixCoordOuter.y,roundingPower),1.0/roundingPower);
      float h = (thick / 2.0);

	    if (dist < radius - h) {
            // lower
            float normalized = smoothstep(0.0, 1.0, (dist - radius + thick + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));
            additionalAlpha *= normalized;
            done = true;
        } else if (min(pixCoordOuter.x, pixCoordOuter.y) > 0.0) {
            // higher
            float normalized = 1.0 - smoothstep(0.0, 1.0, (distOuter - radiusOuter + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));
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
