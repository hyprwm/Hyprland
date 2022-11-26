#pragma once

#include <string>

// makes a stencil without corners
inline const std::string FRAGBORDER1 = R"#(
precision mediump float;
varying vec4 v_color;
varying vec2 v_texcoord;

uniform vec2 topLeft;
uniform vec2 fullSize;
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

    float sine = sin(angle);
    float progress = (normalizedCoord[1] * sine + normalizedCoord[0] * (1.0 - sine)) * float(gradientLength - 1);
    int bottom = int(floor(progress));
    int top = bottom + 1;

    return gradient[top] * (progress - float(bottom)) + gradient[bottom] * (float(top) - progress);
}

void main() {

    highp vec2 pixCoord = vec2(gl_FragCoord);
    vec2 originalPixCoord = fullSize * v_texcoord;

    vec4 pixColor = getColorForCoord(v_texcoord);
    pixColor[3] *= alpha;

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
	        distances += float(len < radius + 0.5 && len > radius - thick);
            len = length(pixCoord + vec2(0.75, 0.25));
            distances += float(len < radius + 0.5 && len > radius - thick);
            len = length(pixCoord + vec2(0.25, 0.75));
            distances += float(len < radius + 0.5 && len > radius - thick);
            len = length(pixCoord + vec2(0.75, 0.75));
            distances += float(len < radius + 0.5 && len > radius - thick);

	        if (distances == 0.0)
		        discard;

	        distances /= 4.0;

	        pixColor = pixColor * distances;
        } else if (dist > radius || dist < radius - thick)
            discard;

        done = true;
    }

    // now check for other shit
    if (!done) {
        // distance to all straight bb borders
        float distanceT = originalPixCoord[1];
        float distanceB = fullSize[1] - originalPixCoord[1];
        float distanceL = originalPixCoord[0];
        float distanceR = fullSize[0] - originalPixCoord[0];

        // get the smallest
        float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));

        if (smallest > thick)
            discard;
    }

    if (pixColor[3] == 0.0)
        discard;

    gl_FragColor = pixColor;
}
)#";
