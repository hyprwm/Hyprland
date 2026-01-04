#version 300 es

#extension GL_ARB_shading_language_include : enable
precision highp float;
in vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;
uniform sampler2D discardTex;
uniform float alpha;

#include "rounding.glsl"

uniform int discardOpaque;
uniform int discardAlpha;
uniform int discardTexEnabled;
uniform vec4 discardTexCoords;
uniform float discardAlphaValue;

uniform int applyTint;
uniform vec3 tint;

layout(location = 0) out vec4 fragColor;
void main() {

    vec4 pixColor = texture(tex, v_texcoord);

    if (discardTexEnabled == 1) {
        vec2 coord = vec2((v_texcoord.x - discardTexCoords[0]) * (discardTexCoords[1] - discardTexCoords[0]), (v_texcoord.y - discardTexCoords[2]) * (discardTexCoords[3] - discardTexCoords[2]));
        vec4 texColor = texture(discardTex, coord);

        if (discardOpaque == 1 && texColor[3] * alpha == 1.0)
            discard;

        if (discardAlpha == 1 && texColor[3] <= discardAlphaValue)
            discard;
    } else {
        if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
            discard;

        if (discardAlpha == 1 && pixColor[3] <= discardAlphaValue)
            discard;
    }

    if (applyTint == 1) {
	    pixColor[0] = pixColor[0] * tint[0];
	    pixColor[1] = pixColor[1] * tint[1];
	    pixColor[2] = pixColor[2] * tint[2];
    }

    if (radius > 0.0)
    	pixColor = rounding(pixColor);

    fragColor = pixColor * alpha;
}
