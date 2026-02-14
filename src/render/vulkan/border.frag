#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;

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
	float radius;
	float power;
	vec2 topLeft;
	vec2 fullSize;
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
    vec2 pixCoord = vec2(gl_FragCoord);
    vec2 pixCoordOuter = pixCoord;
    vec2 originalPixCoord = v_texcoord;
    originalPixCoord *= data.fullSizeUntransformed;
    float additionalAlpha = 1.0;

    vec4 pixColor = vec4(1.0, 1.0, 1.0, 1.0);

    bool done = false;

    pixCoord -= data.topLeft + data.fullSize * 0.5;
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoordOuter = pixCoord;
    pixCoord -= data.fullSize * 0.5 - data.radius;
    pixCoordOuter -= data.fullSize * 0.5 - data.radiusOuter;

    // center the pixes don't make it top-left
    pixCoord += vec2(1.0, 1.0) / data.fullSize;
    pixCoordOuter += vec2(1.0, 1.0) / data.fullSize;

    if (min(pixCoord.x, pixCoord.y) > 0.0 && data.radius > 0.0) {
	    float dist = pow(pow(pixCoord.x,data.power)+pow(pixCoord.y,data.power),1.0/data.power);
	    float distOuter = pow(pow(pixCoordOuter.x,data.power)+pow(pixCoordOuter.y,data.power),1.0/data.power);
        float h = (data.thick / 2.0);

	    if (dist < data.radius - h) {
            // lower
            float normalized = smoothstep(0.0, 1.0, (dist - data.radius + data.thick + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));
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