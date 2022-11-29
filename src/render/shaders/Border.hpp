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
uniform float radius;
uniform float thick;
uniform int primitiveMultisample;

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
    highp vec2 originalPixCoord = v_texcoord;
    originalPixCoord *= fullSizeUntransformed;

    vec4 pixColor = vec4(1.0, 1.0, 1.0, 1.0);

    bool done = false;

    pixCoord -= topLeft + fullSize * 0.5;
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoord -= fullSize * 0.5 - radius;

    if (min(pixCoord.x, pixCoord.y) > 0.0 && radius > 0.0) {

	    float dist = length(pixCoord);

	    if (dist > radius + 1.0 || dist < radius - thick - 1.0)
	        discard;

	    if (primitiveMultisample == 1 && (dist > radius - 1.0 || dist < radius - thick + 1.0)) {
	        float distances = 0.0;
            float len = length(pixCoord + vec2(0.25, 0.25));
	        distances += float(len < radius && len > radius - thick);
            len = length(pixCoord + vec2(0.75, 0.25));
            distances += float(len < radius && len > radius - thick);
            len = length(pixCoord + vec2(0.25, 0.75));
            distances += float(len < radius && len > radius - thick);
            len = length(pixCoord + vec2(0.75, 0.75));
            distances += float(len < radius && len > radius - thick);

	        if (distances == 0.0)
		        discard;

	        distances /= 4.0;

	        pixColor[3] *= distances;
        } else if (dist > radius || dist < radius - thick)
            discard;

        done = true;
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

    if (pixColor[3] == 0.0)
        discard;

    float pixColor3 = pixColor[3];
    pixColor = getColorForCoord(v_texcoord);
    pixColor[3] *= alpha * pixColor3;

    gl_FragColor = pixColor;
}
)#";
