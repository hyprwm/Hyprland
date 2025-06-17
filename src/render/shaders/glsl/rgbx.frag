#version 300 es
#extension GL_ARB_shading_language_include : enable
precision highp float;
in vec2 v_texcoord;
uniform sampler2D tex;
uniform float alpha;

#include "rounding.glsl"

uniform int discardOpaque;
uniform int discardAlpha;
uniform int discardAlphaValue;

uniform int applyTint;
uniform vec3 tint;

layout(location = 0) out vec4 fragColor;
void main() {

    if (discardOpaque == 1 && alpha == 1.0)
	discard;

    vec4 pixColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0);

    if (applyTint == 1) {
	    pixColor[0] = pixColor[0] * tint[0];
	    pixColor[1] = pixColor[1] * tint[1];
	    pixColor[2] = pixColor[2] * tint[2];
    }

    if (radius > 0.0)
		pixColor = rounding(pixColor);

    fragColor = pixColor * alpha;
}
