#pragma once

#include <string>

// makes a stencil without corners
inline const std::string FRAGBORDER1 = R"#(
precision mediump float;
varying vec4 v_color;
varying vec2 v_texcoord;

uniform vec2 topLeft;
uniform vec2 bottomRight;
uniform vec2 fullSize;
uniform float radius;
uniform float thick;
uniform int primitiveMultisample;

float getOpacityForPixAndCorner(vec2 pix, vec2 corner) {

    if (primitiveMultisample == 0) {
        float dis = distance(pix + vec2(0.5, 0.5), corner);
        return dis < radius && dis > radius - thick ? 1.0 : 0.0;
    }

    float distance1 = distance(pix + vec2(0.25, 0.25), corner);
    float distance2 = distance(pix + vec2(0.75, 0.25), corner);
    float distance3 = distance(pix + vec2(0.25, 0.75), corner);
    float distance4 = distance(pix + vec2(0.75, 0.75), corner);

    float v1 = distance1 < radius && distance1 > radius - thick ? 1.0 : 0.0;
    float v2 = distance2 < radius && distance2 > radius - thick ? 1.0 : 0.0;
    float v3 = distance3 < radius && distance3 > radius - thick ? 1.0 : 0.0;
    float v4 = distance4 < radius && distance4 > radius - thick ? 1.0 : 0.0;

    return (v1 + v2 + v3 + v4) / 4.0;
}

void main() {

    vec2 pixCoord = fullSize * v_texcoord;

    vec4 pixColor = v_color;

    bool done = false;
    
    // check for edges
    if (pixCoord[0] < topLeft[0]) {
        if (pixCoord[1] < topLeft[1]) {
            // top left
            pixColor[3] = pixColor[3] * getOpacityForPixAndCorner(pixCoord, topLeft + vec2(1,1));
            done = true;
        } else if (pixCoord[1] > bottomRight[1]) {
            // bottom left
            pixColor[3] = pixColor[3] * getOpacityForPixAndCorner(pixCoord, vec2(topLeft[0] + 1.0, bottomRight[1]));
            done = true;
        }
    } else if (pixCoord[0] > bottomRight[0]) {
        if (pixCoord[1] < topLeft[1]) {
            // top right
            pixColor[3] = pixColor[3] * getOpacityForPixAndCorner(pixCoord, vec2(bottomRight[0], topLeft[1] + 1.0));
            done = true;
        } else if (pixCoord[1] > bottomRight[1]) {
            // bottom right
            pixColor[3] = pixColor[3] * getOpacityForPixAndCorner(pixCoord, bottomRight);
            done = true;
        }
    }

    // now check for other shit
    if (!done) {
        // distance to all straight bb borders
        float distanceT = pixCoord[1];
        float distanceB = fullSize[1] - pixCoord[1];
        float distanceL = pixCoord[0];
        float distanceR = fullSize[0] - pixCoord[0];

        // get the smallest
        float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));

        if (smallest > thick) {
            discard; return;
        }
    }

    if (pixColor[3] == 0.0) {
        discard; return;
    }

    gl_FragColor = pixColor;
}
)#";