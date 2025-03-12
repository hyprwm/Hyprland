#extension GL_ARB_shading_language_include : enable
#extension GL_OES_EGL_image_external : require

precision highp float;
varying vec2 v_texcoord;
uniform samplerExternalOES texture0;
uniform float alpha;

#include "rounding.glsl"

uniform int discardOpaque;
uniform int discardAlpha;
uniform int discardAlphaValue;

uniform int applyTint;
uniform vec3 tint;

void main() {

    vec4 pixColor = texture2D(texture0, v_texcoord);

    if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
	discard;

    if (applyTint == 1) {
	pixColor[0] = pixColor[0] * tint[0];
	pixColor[1] = pixColor[1] * tint[1];
	pixColor[2] = pixColor[2] * tint[2];
    }

    if (radius > 0.0)
		pixColor = rounding(pixColor);

    gl_FragColor = pixColor * alpha;
}
