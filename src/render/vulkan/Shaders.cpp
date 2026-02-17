#include "Shaders.hpp"
#include "Shader.hpp"
#include "render/vulkan/types.hpp"

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

CVkShaders::CVkShaders(WP<CHyprVulkanDevice> device) : IDeviceUser(device) {
    m_vert   = makeShared<CVkShader>(device, VERT_SRC, sizeof(SVkVertShaderData), SH_VERT);
    m_frag   = makeShared<CVkShader>(device, FRAG_SRC, sizeof(SVkFragShaderData), SH_FRAG);
    m_border = makeShared<CVkShader>(device, BORDER_FRAG_SRC, sizeof(SVkBorderShaderData), SH_FRAG);
    m_rect   = makeShared<CVkShader>(device, RECT_FRAG_SRC, sizeof(SVkRectShaderData), SH_FRAG);
    m_shadow = makeShared<CVkShader>(device, SHADOW_FRAG_SRC, sizeof(SVkShadowShaderData), SH_FRAG);
}
