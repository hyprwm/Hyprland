#include "Shaders.hpp"
#include "Shader.hpp"
#include "render/vulkan/Vulkan.hpp"
#include "render/vulkan/types.hpp"
#include "render/vulkan/utils.hpp"

#define VERT_SRC                                                                                                                                                                   \
    R"#(
#version 450

// we use a mat4 since it uses the same size as mat3 due to
// alignment. Easier to deal with (tighly-packed) mat4 though.
layout(push_constant, row_major) uniform UBO {
	mat4 proj;
	vec2 uv_offset;
	vec2 uv_size;
} data;

layout(location = 0) out vec2 uv;

void main() {
	vec2 pos = vec2(float((gl_VertexIndex + 1) & 2) * 0.5f,
		float(gl_VertexIndex & 2) * 0.5f);
	uv = data.uv_offset + pos * data.uv_size;
	gl_Position = data.proj * vec4(pos, 0.0, 1.0);
}
)#"

#define FRAG_SRC                                                                                                                                                                   \
    R"#(
#version 450

layout(set = 0, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(push_constant, row_major) uniform UBO {
	layout(offset = 80) mat4 matrix;
	float alpha;
	float luminance_multiplier;
} data;

layout (constant_id = 0) const int TEXTURE_TRANSFORM = 0;

#define TEXTURE_TRANSFORM_IDENTITY 0
#define TEXTURE_TRANSFORM_SRGB 1
#define TEXTURE_TRANSFORM_ST2084_PQ 2
#define TEXTURE_TRANSFORM_GAMMA22 3
#define TEXTURE_TRANSFORM_BT1886 4

float srgb_channel_to_linear(float x) {
	return mix(x / 12.92,
		pow((x + 0.055) / 1.055, 2.4),
		x > 0.04045);
}

vec3 srgb_color_to_linear(vec3 color) {
	return vec3(
		srgb_channel_to_linear(color.r),
		srgb_channel_to_linear(color.g),
		srgb_channel_to_linear(color.b)
	);
}

vec3 pq_color_to_linear(vec3 color) {
	float inv_m1 = 1 / 0.1593017578125;
	float inv_m2 = 1 / 78.84375;
	float c1 = 0.8359375;
	float c2 = 18.8515625;
	float c3 = 18.6875;
	vec3 num = max(pow(color, vec3(inv_m2)) - c1, 0);
	vec3 denom = c2 - c3 * pow(color, vec3(inv_m2));
	return pow(num / denom, vec3(inv_m1));
}

vec3 bt1886_color_to_linear(vec3 color) {
	float Lmin = 0.01;
	float Lmax = 100.0;
	float lb = pow(Lmin, 1.0 / 2.4);
	float lw = pow(Lmax, 1.0 / 2.4);
	float a  = pow(lw - lb, 2.4);
	float b  = lb / (lw - lb);
	vec3 L = a * pow(color + vec3(b), vec3(2.4));
	return (L - Lmin) / (Lmax - Lmin);
}

void main() {
	vec4 in_color = textureLod(tex, uv, 0);

	// Convert from pre-multiplied alpha to straight alpha
	float alpha = in_color.a;
	vec3 rgb;
	if (alpha == 0) {
		rgb = vec3(0);
	} else {
		rgb = in_color.rgb / alpha;
	}

	if (TEXTURE_TRANSFORM != TEXTURE_TRANSFORM_IDENTITY) {
		rgb = max(rgb, vec3(0));
	}
	if (TEXTURE_TRANSFORM == TEXTURE_TRANSFORM_SRGB) {
		rgb = srgb_color_to_linear(rgb);
	} else if (TEXTURE_TRANSFORM == TEXTURE_TRANSFORM_ST2084_PQ) {
		rgb = pq_color_to_linear(rgb);
	} else if (TEXTURE_TRANSFORM == TEXTURE_TRANSFORM_GAMMA22) {
		rgb = pow(rgb, vec3(2.2));
	} else if (TEXTURE_TRANSFORM == TEXTURE_TRANSFORM_BT1886) {
		rgb = bt1886_color_to_linear(rgb);
	}

	rgb *= data.luminance_multiplier;

	rgb = mat3(data.matrix) * rgb;

	// Back to pre-multiplied alpha
	out_color = vec4(rgb * alpha, alpha);

	out_color *= data.alpha;
}

)#"

#define BORDER_FRAG_SRC                                                                                                                                                            \
    R"#(#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;

struct sRounding {
    float radius;
	float power;
	vec2 topLeft;
	vec2 fullSize;
};

layout(push_constant, row_major) uniform UBO {
	layout(offset = 80) vec2 fullSizeUntransformed;
	float radiusOuter;
	float thick;
	// vec4 gradient[10];
	// vec4 gradient2[10];
    vec4 gradient[1];
	vec4 gradient2[1];
	int gradientLength;
	int gradient2Length;
	float angle;
	float angle2;
	float gradientLerp;
	float alpha;
	sRounding rounding;
} data;

#define M_PI 3.1415926535897932384626433832795
#define SMOOTHING_CONSTANT (M_PI / 5.34665792551)
#define CM_TRANSFER_FUNCTION_GAMMA22 1

vec3 fromLinearRGB(vec3 color, int tf) {
	return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

vec4 okLabAToSrgb(vec4 lab) {
    float l = pow(lab[0] + lab[1] * 0.3963377774 + lab[2] * 0.2158037573, 3.0);
    float m = pow(lab[0] + lab[1] * (-0.1055613458) + lab[2] * (-0.0638541728), 3.0);
    float s = pow(lab[0] + lab[1] * (-0.0894841775) + lab[2] * (-1.2914855480), 3.0);

    return vec4(fromLinearRGB(
                vec3(
                        l * 4.0767416621 + m * -3.3077115913 + s * 0.2309699292,
                        l * (-1.2684380046) + m * 2.6097574011 + s * (-0.3413193965),
                        l * (-0.0041960863) + m * (-0.7034186147) + s * 1.7076147010
                ), CM_TRANSFER_FUNCTION_GAMMA22
        ), lab[3]);
}

vec4 getOkColorForCoordArray1(vec2 normalizedCoord) {
    if (data.gradientLength < 2)
        return data.gradient[0];

    float finalAng = 0.0;

    if (data.angle > 4.71 /* 270 deg */) {
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = 6.28 - data.angle;
    } else if (data.angle > 3.14 /* 180 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = data.angle - 3.14;
    } else if (data.angle > 1.57 /* 90 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        finalAng = 3.14 - data.angle;
    } else {
        finalAng = data.angle;
    }

    float sine = sin(finalAng);

    float progress = (normalizedCoord[1] * sine + normalizedCoord[0] * (1.0 - sine)) * float(data.gradientLength - 1);
    int bottom = int(floor(progress));
    int top = bottom + 1;

    return data.gradient[top] * (progress - float(bottom)) + data.gradient[bottom] * (float(top) - progress);
}

vec4 getOkColorForCoordArray2(vec2 normalizedCoord) {
    if (data.gradient2Length < 2)
        return data.gradient2[0];

    float finalAng = 0.0;

    if (data.angle2 > 4.71 /* 270 deg */) {
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = 6.28 - data.angle;
    } else if (data.angle2 > 3.14 /* 180 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = data.angle - 3.14;
    } else if (data.angle2 > 1.57 /* 90 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        finalAng = 3.14 - data.angle2;
    } else {
        finalAng = data.angle2;
    }

    float sine = sin(finalAng);

    float progress = (normalizedCoord[1] * sine + normalizedCoord[0] * (1.0 - sine)) * float(data.gradient2Length - 1);
    int bottom = int(floor(progress));
    int top = bottom + 1;

    return data.gradient2[top] * (progress - float(bottom)) + data.gradient2[bottom] * (float(top) - progress);
}

vec4 getColorForCoord(vec2 normalizedCoord) {
    vec4 result1 = getOkColorForCoordArray1(normalizedCoord);

    if (data.gradient2Length <= 0)
        return okLabAToSrgb(result1);

    vec4 result2 = getOkColorForCoordArray2(normalizedCoord);

    return okLabAToSrgb(mix(result1, result2, data.gradientLerp));
}


layout(location = 0) out vec4 fragColor;
void main() {
    vec2 originalPixCoord = v_texcoord * data.fullSizeUntransformed;
    vec2 pixCoord = originalPixCoord;
    float additionalAlpha = 1.0;

    vec4 pixColor = vec4(1.0, 1.0, 1.0, 1.0);

    bool done = false;

    pixCoord -= data.fullSizeUntransformed * 0.5;
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    vec2 pixCoordOuter = pixCoord;
    pixCoord -= data.fullSizeUntransformed * 0.5 - data.rounding.radius;
    pixCoordOuter -= data.fullSizeUntransformed * 0.5 - data.radiusOuter;

    // center the pixes don't make it top-left
    pixCoord += vec2(1.0, 1.0) / data.fullSizeUntransformed;
    pixCoordOuter += vec2(1.0, 1.0) / data.fullSizeUntransformed;


    if (min(pixCoord.x, pixCoord.y) > 0.0 && data.rounding.radius > 0.0) {
	    float dist = pow(pow(pixCoord.x,data.rounding.power)+pow(pixCoord.y,data.rounding.power),1.0/data.rounding.power);
	    float distOuter = pow(pow(pixCoordOuter.x,data.rounding.power)+pow(pixCoordOuter.y,data.rounding.power),1.0/data.rounding.power);
        float h = (data.thick / 2.0);

	    if (dist < data.rounding.radius - h) {
            // lower
            float normalized = smoothstep(0.0, 1.0, (dist - data.rounding.radius + data.thick + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));
            additionalAlpha *= normalized;
            done = true;
        } else if (min(pixCoordOuter.x, pixCoordOuter.y) > 0.0) {
            // higher
            float normalized = 1.0 - smoothstep(0.0, 1.0, (distOuter - data.radiusOuter + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));
            additionalAlpha *= normalized;
            done = true;
        } else if (distOuter < data.radiusOuter - h) {
            additionalAlpha = 1.0;
            done = true;
        }
    }

    // now check for other shit
    if (!done) {
        // distance to all straight bb borders
        float distanceT = originalPixCoord[1];
        float distanceB = data.fullSizeUntransformed[1] - originalPixCoord[1];
        float distanceL = originalPixCoord[0];
        float distanceR = data.fullSizeUntransformed[0] - originalPixCoord[0];

        // get the smallest
        float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));

        if (smallest > data.thick)
            discard;
    }

    if (additionalAlpha == 0.0)
        discard;

    pixColor = getColorForCoord(v_texcoord);
    pixColor.rgb *= pixColor[3];

    pixColor *= data.alpha * additionalAlpha;

    fragColor = pixColor;
}

)#"

#define RECT_FRAG_SRC                                                                                                                                                              \
    R"#(#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;

// smoothing constant for the edge: more = blurrier, but smoother
#define M_PI 3.1415926535897932384626433832795
#define SMOOTHING_CONSTANT (M_PI / 5.34665792551)

struct SRounding {
    float radius;
	float power;
	vec2 topLeft;
	vec2 fullSize;
};

layout(push_constant, row_major) uniform UBO {
	layout(offset = 80) vec4 v_color;
	SRounding rounding;
} data;

vec4 rounding(vec4 color) {
    vec2 pixCoord = vec2(gl_FragCoord);
    pixCoord -= data.rounding.topLeft + data.rounding.fullSize * 0.5;
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoord -= data.rounding.fullSize * 0.5 - data.rounding.radius;
    pixCoord += vec2(1.0, 1.0) / data.rounding.fullSize; // center the pix don't make it top-left

    if (pixCoord.x + pixCoord.y > data.rounding.radius) {
        float dist = pow(pow(pixCoord.x, data.rounding.power) + pow(pixCoord.y, data.rounding.power), 1.0/data.rounding.power);

        if (dist > data.rounding.radius + SMOOTHING_CONSTANT)
            discard;

        float normalized = 1.0 - smoothstep(0.0, 1.0, (dist - data.rounding.radius + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));

        color *= normalized;
    }

    return color;
}


layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = data.v_color;

    if (data.rounding.radius > 0.0) 
        pixColor = rounding(pixColor);

    fragColor = pixColor;
}

)#"

#define SHADOW_FRAG_SRC                                                                                                                                                            \
    R"#(#version 450
precision highp float;
layout(location = 0) in vec2 v_texcoord;

struct sRounding {
    float radius;
	float power;
	vec2 topLeft;
	vec2 fullSize;
};

layout(push_constant, row_major) uniform UBO {
	layout(offset = 80) vec4 v_color;
    vec2 bottomRight;
    float range;
    float shadowPower;
	sRounding rounding;
} data;

float pixAlphaRoundedDistance(float distanceToCorner) {
     if (distanceToCorner > data.rounding.radius) {
        return 0.0;
    }

    if (distanceToCorner > data.rounding.radius - data.range) {
        return pow((data.range - (distanceToCorner - data.rounding.radius + data.range)) / data.range, data.shadowPower); // i think?
    }

    return 1.0;
}

float modifiedLength(vec2 a) {
    return pow(pow(abs(a.x),data.rounding.power)+pow(abs(a.y),data.rounding.power),1.0/data.rounding.power);
}

layout(location = 0) out vec4 fragColor;
void main() {

	vec4 pixColor = data.v_color;
    float originalAlpha = pixColor[3];

    bool done = false;

	vec2 pixCoord = data.rounding.fullSize * v_texcoord;

    // ok, now we check the distance to a border.

    if (pixCoord[0] < data.rounding.topLeft[0]) {
        if (pixCoord[1] < data.rounding.topLeft[1]) {
            // top left
            pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - data.rounding.topLeft));
            done = true;
        } else if (pixCoord[1] > data.bottomRight[1]) {
            // bottom left
            pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - vec2(data.rounding.topLeft[0], data.bottomRight[1])));
            done = true;
        }
    } else if (pixCoord[0] > data.bottomRight[0]) {
        if (pixCoord[1] < data.rounding.topLeft[1]) {
            // top right
            pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - vec2(data.bottomRight[0], data.rounding.topLeft[1])));
            done = true;
        } else if (pixCoord[1] > data.bottomRight[1]) {
            // bottom right
            pixColor[3] = pixColor[3] * pixAlphaRoundedDistance(modifiedLength(pixCoord - data.bottomRight));
            done = true;
        }
    }

    if (!done) {
        // distance to all straight bb borders
        float distanceT = pixCoord[1];
        float distanceB = data.rounding.fullSize[1] - pixCoord[1];
        float distanceL = pixCoord[0];
        float distanceR = data.rounding.fullSize[0] - pixCoord[0];

        // get the smallest
        float smallest = min(min(distanceT, distanceB), min(distanceL, distanceR));

        if (smallest < data.range) {
            pixColor[3] = pixColor[3] * pow((smallest / data.range), data.shadowPower);
        }
    }

    if (pixColor[3] == 0.0) {
        discard; return;
    }

    // premultiply
    pixColor.rgb *= pixColor[3];

	fragColor = pixColor;
}
)#"

#define MATTE_FRAG_SRC                                                                                                                                                             \
    R"#(#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 0, binding = 1) uniform sampler2D texMatte;

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = texture(tex, v_texcoord) * texture(texMatte, v_texcoord)[0]; // I know it only uses R, but matte should be black/white anyways.
}

)#"

#define PASS_FRAG_SRC                                                                                                                                                              \
    R"#(#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = texture(tex, v_texcoord);
}

)#"

#define PREPARE_FRAG_SRC                                                                                                                                                           \
    R"#(#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant, row_major) uniform UBO {
	layout(offset = 80) float contrast;
    float brightness;
} data;

#define contrast data.contrast
#define brightness data.brightness

vec3 gain(vec3 x, float k) {
    vec3 t = step(0.5, x);
    vec3 y = mix(x, 1.0 - x, t);
    vec3 a = 0.5 * pow(2.0 * y, vec3(k));
    return mix(a, 1.0 - a, t);
}

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = texture(tex, v_texcoord);

    // contrast
    if (contrast != 1.0)
        pixColor.rgb = gain(pixColor.rgb, contrast);

    // brightness
    pixColor.rgb *= max(1.0, brightness);

    fragColor = pixColor;
}

)#"

#define BLUR1_FRAG_SRC                                                                                                                                                             \
    R"#(#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant, row_major) uniform UBO {
	layout(offset = 80) float radius;
    vec2 halfpixel;
    int passes;
    float vibrancy;
    float vibrancy_darkness;
} data;

#define radius data.radius
#define halfpixel data.halfpixel
#define passes data.passes
#define vibrancy data.vibrancy
#define vibrancy_darkness data.vibrancy_darkness

// see http://alienryderflex.com/hsp.html
const float Pr = 0.299;
const float Pg = 0.587;
const float Pb = 0.114;

// Y is "v" ( brightness ). X is "s" ( saturation )
// see https://www.desmos.com/3d/a88652b9a4
// Determines if high brightness or high saturation is more important
const float a = 0.93;
const float b = 0.11;
const float c = 0.66; //  Determines the smoothness of the transition of unboosted to boosted colors
//

// http://www.flong.com/archive/texts/code/shapers_circ/
float doubleCircleSigmoid(float x, float a) {
    a = clamp(a, 0.0, 1.0);

    float y = .0;
    if (x <= a) {
        y = a - sqrt(a * a - x * x);
    } else {
        y = a + sqrt(pow(1. - a, 2.) - pow(x - 1., 2.));
    }
    return y;
}

vec3 rgb2hsl(vec3 col) {
    float red   = col.r;
    float green = col.g;
    float blue  = col.b;

    float minc  = min(col.r, min(col.g, col.b));
    float maxc  = max(col.r, max(col.g, col.b));
    float delta = maxc - minc;

    float lum = (minc + maxc) * 0.5;
    float sat = 0.0;
    float hue = 0.0;

    if (lum > 0.0 && lum < 1.0) {
        float mul = (lum < 0.5) ? (lum) : (1.0 - lum);
        sat       = delta / (mul * 2.0);
    }

    if (delta > 0.0) {
        vec3  maxcVec = vec3(maxc);
        vec3  masks = vec3(equal(maxcVec, col)) * vec3(notEqual(maxcVec, vec3(green, blue, red)));
        vec3  adds = vec3(0.0, 2.0, 4.0) + vec3(green - blue, blue - red, red - green) / delta;

        hue += dot(adds, masks);
        hue /= 6.0;

        if (hue < 0.0)
            hue += 1.0;
    }

    return vec3(hue, sat, lum);
}

vec3 hsl2rgb(vec3 col) {
    const float onethird = 1.0 / 3.0;
    const float twothird = 2.0 / 3.0;
    const float rcpsixth = 6.0;

    float       hue = col.x;
    float       sat = col.y;
    float       lum = col.z;

    vec3        xt = vec3(0.0);

    if (hue < onethird) {
        xt.r = rcpsixth * (onethird - hue);
        xt.g = rcpsixth * hue;
        xt.b = 0.0;
    } else if (hue < twothird) {
        xt.r = 0.0;
        xt.g = rcpsixth * (twothird - hue);
        xt.b = rcpsixth * (hue - onethird);
    } else
        xt = vec3(rcpsixth * (hue - twothird), 0.0, rcpsixth * (1.0 - hue));

    xt = min(xt, 1.0);

    float sat2   = 2.0 * sat;
    float satinv = 1.0 - sat;
    float luminv = 1.0 - lum;
    float lum2m1 = (2.0 * lum) - 1.0;
    vec3  ct     = (sat2 * xt) + satinv;

    vec3  rgb;
    if (lum >= 0.5)
        rgb = (luminv * ct) + lum2m1;
    else
        rgb = lum * ct;

    return rgb;
}

layout(location = 0) out vec4 fragColor;
void main() {
    vec2 uv = v_texcoord * 2.0;

    vec4 sum = texture(tex, uv) * 4.0;
    sum += texture(tex, uv - halfpixel.xy * radius);
    sum += texture(tex, uv + halfpixel.xy * radius);
    sum += texture(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius);
    sum += texture(tex, uv - vec2(halfpixel.x, -halfpixel.y) * radius);

    vec4 color = sum / 8.0;

    if (vibrancy == 0.0) {
        fragColor = color;
    } else {
        // Invert it so that it correctly maps to the config setting
        float vibrancy_darkness1 = 1.0 - vibrancy_darkness;

        // Decrease the RGB components based on their perceived brightness, to prevent visually dark colors from overblowing the rest.
        vec3 hsl = rgb2hsl(color.rgb);
        // Calculate perceived brightness, as not boost visually dark colors like deep blue as much as equally saturated yellow
        float perceivedBrightness = doubleCircleSigmoid(sqrt(color.r * color.r * Pr + color.g * color.g * Pg + color.b * color.b * Pb), 0.8 * vibrancy_darkness1);

        float b1        = b * vibrancy_darkness1;
        float boostBase = hsl[1] > 0.0 ? smoothstep(b1 - c * 0.5, b1 + c * 0.5, 1.0 - (pow(1.0 - hsl[1] * cos(a), 2.0) + pow(1.0 - perceivedBrightness * sin(a), 2.0))) : 0.0;

        float saturation = clamp(hsl[1] + (boostBase * vibrancy) / float(passes), 0.0, 1.0);

        vec3  newColor = hsl2rgb(vec3(hsl[0], saturation, hsl[2]));

        fragColor = vec4(newColor, color[3]);
    }
}


)#"

#define BLUR2_FRAG_SRC                                                                                                                                                             \
    R"#(#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant, row_major) uniform UBO {
	layout(offset = 80) float radius;
    vec2 halfpixel;
} data;

#define radius data.radius
#define halfpixel data.halfpixel


layout(location = 0) out vec4 fragColor;
void main() {
    vec2 uv = v_texcoord / 2.0;

    vec4 sum = texture(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * radius);

    sum += texture(tex, uv + vec2(-halfpixel.x,  halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(0.0,           halfpixel.y * 2.0) * radius);
    sum += texture(tex, uv + vec2(halfpixel.x,   halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * radius);
    sum += texture(tex, uv + vec2(halfpixel.x,  -halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(0.0,          -halfpixel.y * 2.0) * radius);
    sum += texture(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;

    fragColor = sum / 12.0;
}

)#"

#define FINISH_FRAG_SRC                                                                                                                                                            \
    R"#(#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant, row_major) uniform UBO {
	layout(offset = 80) float noise;
    float brightness;
} data;

#define noise data.noise
#define brightness data.brightness

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 1689.1984);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = texture(tex, v_texcoord);

    // noise
    float noiseHash   = hash(v_texcoord);
    float noiseAmount = noiseHash - 0.5;
    pixColor.rgb += noiseAmount * noise;

    // brightness
    pixColor.rgb *= min(1.0, brightness);

    fragColor = pixColor;
}


)#"

CVkShaders::CVkShaders(WP<CHyprVulkanDevice> device) : IDeviceUser(device) {
    m_vert   = makeShared<CVkShader>(device, VERT_SRC, sizeof(SVkVertShaderData), SH_VERT, "vert");
    m_frag   = makeShared<CVkShader>(device, FRAG_SRC, sizeof(SVkFragShaderData), SH_FRAG, "frag");
    m_border = makeShared<CVkShader>(device, BORDER_FRAG_SRC, sizeof(SVkBorderShaderData), SH_FRAG, "border");
    m_rect   = makeShared<CVkShader>(device, RECT_FRAG_SRC, sizeof(SVkRectShaderData), SH_FRAG, "rect");
    m_shadow = makeShared<CVkShader>(device, SHADOW_FRAG_SRC, sizeof(SVkShadowShaderData), SH_FRAG, "shadow");
    m_matte  = makeShared<CVkShader>(device, MATTE_FRAG_SRC, 0, SH_FRAG, "matte");
    m_pass   = makeShared<CVkShader>(device, PASS_FRAG_SRC, 0, SH_FRAG, "pass");

    m_prepare = makeShared<CVkShader>(device, PREPARE_FRAG_SRC, sizeof(SVkPrepareShaderData), SH_FRAG, "blur prepare");
    m_blur1   = makeShared<CVkShader>(device, BLUR1_FRAG_SRC, sizeof(SVkBlur1ShaderData), SH_FRAG, "blur 1");
    m_blur2   = makeShared<CVkShader>(device, BLUR2_FRAG_SRC, sizeof(SVkBlur2ShaderData), SH_FRAG, "blur 2");
    m_finish  = makeShared<CVkShader>(device, FINISH_FRAG_SRC, sizeof(SVkFinishShaderData), SH_FRAG, "blur finish");
}
