#version 320 es
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec4 v_color;
in vec2 v_texcoord;

uniform int skipCM;
uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction
uniform mat4x2 sourcePrimaries;
uniform mat4x2 targetPrimaries;

uniform vec2 topLeft;
uniform vec2 bottomRight;
uniform vec2 fullSize;
uniform float radius;
uniform float roundingPower;
uniform float range;
uniform float shadowPower;

#include "CM.glsl"

float pixAlphaRoundedDistance(float distanceToCorner) {
     if (distanceToCorner > radius) {
        return 0.0;
    }

    if (distanceToCorner > radius - range) {
        return pow((range - (distanceToCorner - radius + range)) / range, shadowPower); // i think?
    }

    return 1.0;
}

float modifiedLength(vec2 a) {
    return pow(pow(abs(a.x),roundingPower)+pow(abs(a.y),roundingPower),1.0/roundingPower);
}

layout(location = 0) out vec4 fragColor;
void main() {

	vec4 pixColor = v_color;
    float originalAlpha = pixColor[3];

    bool done = false;

	vec2 pixCoord = fullSize * v_texcoord;

    // ok, now we check the distance to a border.

    if (pixCoord[0] < topLeft[0]) {
        if (pixCoord[1] < topLeft[1]) {
            // top left
            pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - topLeft));
            done = true;
        } else if (pixCoord[1] > bottomRight[1]) {
            // bottom left
            pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - vec2(topLeft[0], bottomRight[1])));
            done = true;
        }
    } else if (pixCoord[0] > bottomRight[0]) {
        if (pixCoord[1] < topLeft[1]) {
            // top right
            pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - vec2(bottomRight[0], topLeft[1])));
            done = true;
        } else if (pixCoord[1] > bottomRight[1]) {
            // bottom right
            pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - bottomRight));
            done = true;
        }
    }

    if (!done) {
        // distance to all straight bb borders
        float distanceT = pixCoord[1];
        float distanceB = fullSize[1] - pixCoord[1];
        float distanceL = pixCoord[0];
        float distanceR = fullSize[0] - pixCoord[0];

        // get the smallest
        float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));

        if (smallest < range) {
            pixColor[3] = pixColor[3] * pow((smallest / range), shadowPower);
        }
    }

    if (pixColor[3] == 0.0) {
        discard; return;
    }

    // premultiply
    pixColor.rgb *= pixColor[3];

	if (skipCM == 0)
        pixColor = doColorManagement(pixColor, sourceTF, sourcePrimaries, targetTF, targetPrimaries);

	fragColor = pixColor;
}