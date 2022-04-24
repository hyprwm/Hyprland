#pragma once

#include <string>

inline const std::string QUADVERTSRC = R"#(
uniform mat3 proj;
uniform vec4 color;
attribute vec2 pos;
attribute vec2 texcoord;
varying vec4 v_color;
varying vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_color = color;
    v_texcoord = texcoord;
})#";

inline const std::string QUADFRAGSRC = R"#(
precision mediump float;
varying vec4 v_color;
varying vec2 v_texcoord;

void main() {
    gl_FragColor = v_color;
})#";

inline const std::string TEXVERTSRC = R"#(
uniform mat3 proj;
attribute vec2 pos;
attribute vec2 texcoord;
varying vec2 v_texcoord;

void main() {
	gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
	v_texcoord = texcoord;
})#";

inline const std::string TEXFRAGSRCRGBA = R"#(
precision mediump float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;
uniform float alpha;

uniform vec2 topLeft;
uniform vec2 bottomRight;
uniform vec2 fullSize;
uniform float radius;

uniform int discardOpaque;

void main() {

	vec4 pixColor = texture2D(tex, v_texcoord);

	if (discardOpaque == 1 && pixColor[3] * alpha == 1.0) {
		discard;
		return;
	}

	vec2 pixCoord = fullSize * v_texcoord;

	if (pixCoord[0] < topLeft[0]) {
		// we're close left
		if (pixCoord[1] < topLeft[1]) {
			// top
			if (distance(topLeft, pixCoord) > radius) {
				discard;
				return;
			}
		} else if (pixCoord[1] > bottomRight[1]) {
			// bottom
			if (distance(vec2(topLeft[0], bottomRight[1]), pixCoord) > radius) {
				discard;
				return;
			}
		}
	}
	else if (pixCoord[0] > bottomRight[0]) {
		// we're close right
		if (pixCoord[1] < topLeft[1]) {
			// top
			if (distance(vec2(bottomRight[0], topLeft[1]), pixCoord) > radius) {
				discard;
				return;
			}
		} else if (pixCoord[1] > bottomRight[1]) {
			// bottom
			if (distance(bottomRight, pixCoord) > radius) {
				discard;
				return;
			}
		}
	}

	gl_FragColor = pixColor * alpha;
})#";

inline const std::string TEXFRAGSRCRGBX = R"#(
precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D tex;
uniform float alpha;

uniform vec2 topLeft;
uniform vec2 bottomRight;
uniform vec2 fullSize;
uniform float radius;

uniform int discardOpaque;

void main() {

	if (discardOpaque == 1 && alpha == 1.0) {
		discard;
		return;
	}
	
	vec2 pixCoord = fullSize * v_texcoord;

	if (pixCoord[0] < topLeft[0]) {
		// we're close left
		if (pixCoord[1] < topLeft[1]) {
			// top
			if (distance(topLeft, pixCoord) > radius) {
				discard;
				return;
			}
		} else if (pixCoord[1] > bottomRight[1]) {
			// bottom
			if (distance(vec2(topLeft[0], bottomRight[1]), pixCoord) > radius) {
				discard;
				return;
			}
		}
	}
	else if (pixCoord[0] > bottomRight[0]) {
		// we're close right
		if (pixCoord[1] < topLeft[1]) {
			// top
			if (distance(vec2(bottomRight[0], topLeft[1]), pixCoord) > radius) {
				discard;
				return;
			}
		} else if (pixCoord[1] > bottomRight[1]) {
			// bottom
			if (distance(bottomRight, pixCoord) > radius) {
				discard;
				return;
			}
		}
	}

	gl_FragColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0) * alpha;
})#";

// thanks to Loadus
// https://www.shadertoy.com/view/Mtl3Rj
// for this pseudo-gaussian blur!
inline const std::string FRAGBLUR1 = R"#(
precision mediump float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float radius;
uniform vec2 resolution;
uniform vec2 halfpixel;

void main() {
    vec4 sum = texture2D(tex, v_texcoord) * 4.0;
    sum += texture2D(tex, v_texcoord - halfpixel.xy * radius);
    sum += texture2D(tex, v_texcoord + halfpixel.xy * radius);
    sum += texture2D(tex, v_texcoord + vec2(halfpixel.x, -halfpixel.y) * radius);
    sum += texture2D(tex, v_texcoord - vec2(halfpixel.x, -halfpixel.y) * radius);
    gl_FragColor = sum / 8.0;
}
)#";

inline const std::string FRAGBLUR2 = R"#(
precision mediump float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float radius;
uniform vec2 resolution;
uniform vec2 halfpixel;

void main() {
    vec4 sum = texture2D(tex, v_texcoord + vec2(-halfpixel.x * 2.0, 0.0) * radius);
    sum += texture2D(tex, v_texcoord + vec2(-halfpixel.x, halfpixel.y) * radius) * 2.0;
    sum += texture2D(tex, v_texcoord + vec2(0.0, halfpixel.y * 2.0) * radius);
    sum += texture2D(tex, v_texcoord + vec2(halfpixel.x, halfpixel.y) * radius) * 2.0;
    sum += texture2D(tex, v_texcoord + vec2(halfpixel.x * 2.0, 0.0) * radius);
    sum += texture2D(tex, v_texcoord + vec2(halfpixel.x, -halfpixel.y) * radius) * 2.0;
    sum += texture2D(tex, v_texcoord + vec2(0.0, -halfpixel.y * 2.0) * radius);
    sum += texture2D(tex, v_texcoord + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;
    gl_FragColor = sum / 12.0;
}
)#";

inline const std::string TEXFRAGSRCEXT = R"#(
#extension GL_OES_EGL_image_external : require

precision mediump float;
varying vec2 v_texcoord;
uniform samplerExternalOES texture0;
uniform float alpha;

uniform vec2 topLeft;
uniform vec2 bottomRight;
uniform vec2 fullSize;
uniform float radius;

uniform int discardOpaque;

void main() {

	vec4 pixColor = texture2D(texture0, v_texcoord);

	if (discardOpaque == 1 && pixColor[3] * alpha == 1.0) {
		discard;
		return;
	}

	vec2 pixCoord = fullSize * v_texcoord;

	if (pixCoord[0] < topLeft[0]) {
		// we're close left
		if (pixCoord[1] < topLeft[1]) {
			// top
			if (distance(topLeft, pixCoord) > radius) {
				discard;
				return;
			}
		} else if (pixCoord[1] > bottomRight[1]) {
			// bottom
			if (distance(vec2(topLeft[0], bottomRight[1]), pixCoord) > radius) {
				discard;
				return;
			}
		}
	}
	else if (pixCoord[0] > bottomRight[0]) {
		// we're close right
		if (pixCoord[1] < topLeft[1]) {
			// top
			if (distance(vec2(bottomRight[0], topLeft[1]), pixCoord) > radius) {
				discard;
				return;
			}
		} else if (pixCoord[1] > bottomRight[1]) {
			// bottom
			if (distance(bottomRight, pixCoord) > radius) {
				discard;
				return;
			}
		}
	}

	gl_FragColor = pixColor * alpha;
})#";
