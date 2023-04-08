#pragma once

#include <string>

inline static constexpr auto ROUNDED_SHADER_FUNC = [](const std::string colorVarName) -> std::string {
    return R"#(

    // branchless baby!
    highp vec2 pixCoord = vec2(gl_FragCoord);
    pixCoord -= topLeft + fullSize * 0.5;
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoord -= fullSize * 0.5 - radius;

    if (pixCoord.x + pixCoord.y > radius) {

	    float dist = length(pixCoord);

	    if (dist > radius)
	        discard;

	    if (primitiveMultisample == 1 && dist > radius - 1.0) {
	        float distances = 0.0;
	        distances += float(length(pixCoord + vec2(0.25, 0.25)) < radius);
	        distances += float(length(pixCoord + vec2(0.75, 0.25)) < radius);
	        distances += float(length(pixCoord + vec2(0.25, 0.75)) < radius);
	        distances += float(length(pixCoord + vec2(0.75, 0.75)) < radius);

	        if (distances == 0.0)
		        discard;

	        distances /= 4.0;

	        )#" +
        colorVarName + R"#( = )#" + colorVarName + R"#( * distances;
        }

    }
)#";
};

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

uniform vec2 topLeft;
uniform vec2 fullSize;
uniform float radius;

uniform int primitiveMultisample;

void main() {

    vec4 pixColor = v_color;

    if (radius > 0.0) {
	)#" +
    ROUNDED_SHADER_FUNC("pixColor") + R"#(
    }

    gl_FragColor = pixColor;
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
uniform vec2 fullSize;
uniform float radius;

uniform int discardOpaque;
uniform int discardAlphaZero;

uniform int applyTint;
uniform vec3 tint;

uniform int primitiveMultisample;

void main() {

    vec4 pixColor = texture2D(tex, v_texcoord);

    if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
	    discard;
    
    if (discardAlphaZero == 1 && pixColor[3] == 0.0)
        discard;

    if (applyTint == 1) {
	    pixColor[0] = pixColor[0] * tint[0];
	    pixColor[1] = pixColor[1] * tint[1];
	    pixColor[2] = pixColor[2] * tint[2];
    }

    )#" +
    ROUNDED_SHADER_FUNC("pixColor") + R"#(

    gl_FragColor = pixColor * alpha;
})#";

inline const std::string TEXFRAGSRCRGBAPASSTHRU = R"#(
precision mediump float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

void main() {
    gl_FragColor = texture2D(tex, v_texcoord);
})#";

inline const std::string TEXFRAGSRCRGBX = R"#(
precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D tex;
uniform float alpha;

uniform vec2 topLeft;
uniform vec2 fullSize;
uniform float radius;

uniform int discardOpaque;
uniform int discardAlphaZero;

uniform int applyTint;
uniform vec3 tint;

uniform int primitiveMultisample;

void main() {

    if (discardOpaque == 1 && alpha == 1.0)
	discard;

    vec4 pixColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0);

    if (applyTint == 1) {
	pixColor[0] = pixColor[0] * tint[0];
	pixColor[1] = pixColor[1] * tint[1];
	pixColor[2] = pixColor[2] * tint[2];
    }

    )#" +
    ROUNDED_SHADER_FUNC("pixColor") + R"#(

    gl_FragColor = pixColor * alpha;
})#";

inline const std::string FRAGBLUR1 = R"#(
#version 100
precision mediump float;
varying mediump vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float radius;
uniform vec2 halfpixel;

void main() {
    vec2 uv = v_texcoord * 2.0;

    vec4 sum = texture2D(tex, uv) * 4.0;
    sum += texture2D(tex, uv - halfpixel.xy * radius);
    sum += texture2D(tex, uv + halfpixel.xy * radius);
    sum += texture2D(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius);
    sum += texture2D(tex, uv - vec2(halfpixel.x, -halfpixel.y) * radius);

    gl_FragColor = sum / 8.0;
}
)#";

inline const std::string FRAGBLUR2 = R"#(
#version 100
precision mediump float;
varying mediump vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float radius;
uniform vec2 halfpixel;

void main() {
    vec2 uv = v_texcoord / 2.0;

    vec4 sum = texture2D(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * radius);

    sum += texture2D(tex, uv + vec2(-halfpixel.x, halfpixel.y) * radius) * 2.0;
    sum += texture2D(tex, uv + vec2(0.0, halfpixel.y * 2.0) * radius);
    sum += texture2D(tex, uv + vec2(halfpixel.x, halfpixel.y) * radius) * 2.0;
    sum += texture2D(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * radius);
    sum += texture2D(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius) * 2.0;
    sum += texture2D(tex, uv + vec2(0.0, -halfpixel.y * 2.0) * radius);
    sum += texture2D(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;

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
uniform vec2 fullSize;
uniform float radius;

uniform int discardOpaque;
uniform int discardAlphaZero;

uniform int applyTint;
uniform vec3 tint;

uniform int primitiveMultisample;

void main() {

    vec4 pixColor = texture2D(texture0, v_texcoord);

    if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
	discard;

    if (applyTint == 1) {
	pixColor[0] = pixColor[0] * tint[0];
	pixColor[1] = pixColor[1] * tint[1];
	pixColor[2] = pixColor[2] * tint[2];
    }

    )#" +
    ROUNDED_SHADER_FUNC("pixColor") + R"#(

    gl_FragColor = pixColor * alpha;
}
)#";

static const std::string FRAGGLITCH = R"#(
precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D tex;
uniform float time; // quirk: time is set to 0 at the beginning, should be around 10 when crash.
uniform float distort;
uniform vec2 screenSize;

float rand(float co) {
    return fract(sin(dot(vec2(co, co), vec2(12.9898, 78.233))) * 43758.5453);
}

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 point) {
    vec2 floored = floor(point);
    vec2 fractal = fract(point);
    fractal = fractal * fractal * (3.0 - 2.0 * fractal);

    float mixed = mix(
        mix(rand(floored), rand(floored + vec2(1.0, 0.0)), fractal.x),
        mix(rand(floored + vec2(0.0,1.0)), rand(floored + vec2(1.0,1.0)), fractal.x), fractal.y);
    return mixed * mixed;
}

void main() {
    float ABERR_OFFSET = 4.0 * (distort / 5.5) * time;
    float TEAR_AMOUNT = 9000.0 * (1.0 - (distort / 5.5));
    float TEAR_BANDS = 108.0 / 2.0 * (distort / 5.5) * 2.0;
    float MELT_AMOUNT = (distort * 8.0) / screenSize.y;

    float NOISE = abs(mod(noise(v_texcoord) * distort * time * 2.771, 1.0)) * time / 10.0;
    if (time < 2.0)
        NOISE = 0.0;

    float offset = (mod(rand(floor(v_texcoord.y * TEAR_BANDS)) * 318.772 * time, 20.0) - 10.0) / TEAR_AMOUNT;

    vec2 blockOffset = vec2(((abs(mod(rand(floor(v_texcoord.x * 37.162)) * 721.43, 100.0))) - 50.0) / 200000.0 * pow(time, 3.0),
                            ((abs(mod(rand(floor(v_texcoord.y * 45.882)) * 733.923, 100.0))) - 50.0) / 200000.0 * pow(time, 3.0));
    if (time < 3.0)
        blockOffset = vec2(0,0);

    float meltSeed = abs(mod(rand(floor(v_texcoord.x * screenSize.x * 17.719)) * 281.882, 1.0));
    if (meltSeed < 0.8) {
        meltSeed = 0.0;
    } else {
        meltSeed *= 25.0 * NOISE;
    }
    float meltAmount = MELT_AMOUNT * meltSeed;

    vec2 pixCoord = vec2(v_texcoord.x + offset + NOISE * 3.0 / screenSize.x + blockOffset.x, v_texcoord.y - meltAmount + 0.02 * NOISE / screenSize.x + NOISE * 3.0 / screenSize.y  + blockOffset.y);

    vec4 pixColor = texture2D(tex, pixCoord);
    vec4 pixColorLeft = texture2D(tex, pixCoord + vec2(ABERR_OFFSET / screenSize.x, 0));
    vec4 pixColorRight = texture2D(tex, pixCoord + vec2(-ABERR_OFFSET / screenSize.x, 0));

    pixColor[0] = pixColorLeft[0];
    pixColor[2] = pixColorRight[2];

    pixColor[0] += distort / 90.0;

    gl_FragColor = pixColor;
}
)#";