R"#(
#version 320 es
//#extension GL_OES_EGL_image_external : require

precision highp float;
in vec2 v_texcoord;
uniform sampler2D tex;
//uniform samplerExternalOES texture0;

uniform int texType; // eTextureType: 0 - rgba, 1 - rgbx, 2 - ext
uniform int skipCM;
uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction
uniform mat4x2 sourcePrimaries;
uniform mat4x2 targetPrimaries;
uniform float maxLuminance;
uniform float dstMaxLuminance;
uniform float dstRefLuminance;

uniform float alpha;

uniform vec2 topLeft;
uniform vec2 fullSize;
uniform float radius;
uniform float roundingPower;

uniform int discardOpaque;
uniform int discardAlpha;
uniform float discardAlphaValue;

uniform int applyTint;
uniform vec3 tint;

//enum eTransferFunction
#define CM_TRANSFER_FUNCTION_BT1886     1
#define CM_TRANSFER_FUNCTION_GAMMA22    2
#define CM_TRANSFER_FUNCTION_GAMMA28    3
#define CM_TRANSFER_FUNCTION_ST240      4
#define CM_TRANSFER_FUNCTION_EXT_LINEAR 5
#define CM_TRANSFER_FUNCTION_LOG_100    6
#define CM_TRANSFER_FUNCTION_LOG_316    7
#define CM_TRANSFER_FUNCTION_XVYCC      8
#define CM_TRANSFER_FUNCTION_SRGB       9
#define CM_TRANSFER_FUNCTION_EXT_SRGB   10
#define CM_TRANSFER_FUNCTION_ST2084_PQ  11
#define CM_TRANSFER_FUNCTION_ST428      12
#define CM_TRANSFER_FUNCTION_HLG        13

// sRGB constants
#define SRGB_POW 2.4
#define SRGB_INV_POW (1.0 / SRGB_POW)
#define SRGB_D_CUT 0.04045
#define SRGB_E_CUT 0.0031308
#define SRGB_LO 12.92
#define SRGB_HI 1.055
#define SRGB_HI_ADD 0.055


// PQ constants
#define PQ_M1 0.1593017578125
#define PQ_M2 78.84375
#define PQ_INV_M1 (1.0 / PQ_M1)
#define PQ_INV_M2 (1.0 / PQ_M2)
#define PQ_C1 0.8359375
#define PQ_C2 18.8515625
#define PQ_C3 18.6875

#define MAX_LUMINANCE 10000.0

// smoothing constant for the edge: more = blurrier, but smoother
#define M_PI 3.1415926535897932384626433832795
#define M_E 2.718281828459045
#define SMOOTHING_CONSTANT (M_PI / 5.34665792551)

vec4 rounding(vec4 color) {
	highp vec2 pixCoord = vec2(gl_FragCoord);
    pixCoord -= topLeft + fullSize * 0.5;
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoord -= fullSize * 0.5 - radius;
    pixCoord += vec2(1.0, 1.0) / fullSize; // center the pix dont make it top-left

    if (pixCoord.x + pixCoord.y > radius) {

		float dist = pow(pow(pixCoord.x, roundingPower) + pow(pixCoord.y, roundingPower), 1.0/roundingPower);

		if (dist > radius + SMOOTHING_CONSTANT)
			discard;

        float normalized = 1.0 - smoothstep(0.0, 1.0, (dist - radius + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));

	    color *= normalized;
    }

	return color;
}

vec4 toLinear(vec4 color, int tf) {
    if (tf == CM_TRANSFER_FUNCTION_EXT_LINEAR)
        return color;
    
    color.rgb /= max(color.a, 0.001);
    switch (tf) {
        case CM_TRANSFER_FUNCTION_ST2084_PQ:
            vec3 E = pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(PQ_INV_M2));
            color.rgb = pow(
                (max(E - PQ_C1, vec3(0.0))) / (PQ_C2 - PQ_C3 * E),
                vec3(PQ_INV_M1)
            );
        case CM_TRANSFER_FUNCTION_GAMMA22:
            color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(2.2));
        case CM_TRANSFER_FUNCTION_GAMMA28:
            color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(2.8));
        case CM_TRANSFER_FUNCTION_BT1886:
        case CM_TRANSFER_FUNCTION_ST240:
        case CM_TRANSFER_FUNCTION_LOG_100:
        case CM_TRANSFER_FUNCTION_LOG_316:
        case CM_TRANSFER_FUNCTION_XVYCC:
        case CM_TRANSFER_FUNCTION_EXT_SRGB:
        case CM_TRANSFER_FUNCTION_ST428:
        case CM_TRANSFER_FUNCTION_HLG:

        case CM_TRANSFER_FUNCTION_SRGB:
        default:
            bvec3 isLow = lessThanEqual(color.rgb, vec3(SRGB_D_CUT));
            vec3 lo = color.rgb / SRGB_LO;
            vec3 hi = pow((color.rgb + SRGB_HI_ADD) / SRGB_HI, vec3(SRGB_POW));
            color.rgb = mix(hi, lo, isLow);
    }
    color.rgb *= color.a;
    return color;
}

vec4 fromLinear(vec4 color, int tf) {
    switch (tf) {
        case CM_TRANSFER_FUNCTION_EXT_LINEAR:
            return color;
        case CM_TRANSFER_FUNCTION_ST2084_PQ:
            vec3 E = pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(PQ_M1));
            return vec4(
                pow(
                    (vec3(PQ_C1) + PQ_C2 * E) / (vec3(1.0) + PQ_C3 * E),
                    vec3(PQ_M2)
                ),
                color[3]
            );
        case CM_TRANSFER_FUNCTION_GAMMA22:
            color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(1.0 / 2.2));
        case CM_TRANSFER_FUNCTION_GAMMA28:
            color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(1.0 / 2.8));
        case CM_TRANSFER_FUNCTION_BT1886:
        case CM_TRANSFER_FUNCTION_ST240:
        case CM_TRANSFER_FUNCTION_LOG_100:
        case CM_TRANSFER_FUNCTION_LOG_316:
        case CM_TRANSFER_FUNCTION_XVYCC:
        case CM_TRANSFER_FUNCTION_EXT_SRGB:
        case CM_TRANSFER_FUNCTION_ST428:
        case CM_TRANSFER_FUNCTION_HLG:

        case CM_TRANSFER_FUNCTION_SRGB:
        default:
            bvec3 isLow = lessThanEqual(color.rgb, vec3(SRGB_E_CUT));
            vec3 lo = color.rgb * SRGB_LO;
            vec3 hi = pow(color.rgb, vec3(SRGB_INV_POW)) * SRGB_HI - SRGB_HI_ADD;
            return vec4(mix(hi, lo, isLow), color[3]);
    }
}

vec3 xy2xyz(vec2 xy) {
    if (xy.y == 0.0)
        return vec3(0.0, 0.0, 0.0);
    
    return vec3(xy.x / xy.y, 1.0, (1.0 - xy.x - xy.y) / xy.y);
}

mat3 primaries2xyz(mat4x2 primaries) {
    vec3 r = xy2xyz(primaries[0]);
    vec3 g = xy2xyz(primaries[1]);
    vec3 b = xy2xyz(primaries[2]);
    vec3 w = xy2xyz(primaries[3]);
    
    mat3 invMat = transpose(inverse(
        mat3(r, g, b)
    ));

    vec3 s = invMat * w;

    return mat3(r * s, g * s, b * s);
}

vec4 convertPrimaries(vec4 color, mat3 src, mat3 dst) {
    mat3 convMat = transpose(src * inverse(dst));
    return vec4(convMat * color.rgb, color[3]);
}

const mat3 BT2020toLMS = mat3(
    0.3592, 0.6976, -0.0358,
    -0.1922, 1.1004, 0.0755,
    0.0070, 0.0749, 0.8434
);
const mat3 LMStoBT2020 = inverse(BT2020toLMS);

const mat3 ICtCpPQ = transpose(mat3(
    2048.0, 2048.0, 0.0,
    6610.0, -13613.0, 7003.0,
    17933.0, -17390.0, -543.0
) / 4096.0);
const mat3 ICtCpPQInv = inverse(ICtCpPQ);

const mat3 ICtCpHLG = transpose(mat3(
    2048.0, 2048.0, 0.0,
    3625.0, -7465.0, 3840.0,
    9500.0, -9212.0, -288.0
) / 4096.0);
const mat3 ICtCpHLGInv = inverse(ICtCpHLG);

vec4 tonemap(vec4 color, mat3 dstXYZ) {
    if (maxLuminance < dstMaxLuminance * 1.01)
        return vec4(clamp(color.rgb, vec3(0.0), vec3(dstMaxLuminance)), color[3]);

    mat3 toLMS = BT2020toLMS * dstXYZ;
    mat3 fromLMS = inverse(dstXYZ) * LMStoBT2020;

    vec3 lms = fromLinear(vec4((toLMS * color.rgb) / MAX_LUMINANCE, 1.0), CM_TRANSFER_FUNCTION_ST2084_PQ).rgb;
    vec3 ICtCp = ICtCpPQ * lms;

    float E = pow(clamp(ICtCp[0], 0.0, 1.0), PQ_INV_M2);
    float luminance = pow(
        (max(E - PQ_C1, 0.0)) / (PQ_C2 - PQ_C3 * E),
        PQ_INV_M1
    ) * MAX_LUMINANCE;

    float srcScale = maxLuminance / dstRefLuminance;
    float dstScale = dstMaxLuminance / dstRefLuminance;

    float minScale = min(srcScale, 1.5);
    float dimming = 1.0 / clamp(minScale / dstScale, 1.0, minScale);
    float refLuminance = dstRefLuminance * dimming;

    float low = min(luminance * dimming, refLuminance);
    float highlight = clamp((luminance / dstRefLuminance - 1.0) / (srcScale - 1.0), 0.0, 1.0);
    float high = log(highlight * (M_E - 1.0) + 1.0) * (dstMaxLuminance - refLuminance);
    luminance = low + high;

    E = pow(clamp(ICtCp[0], 0.0, 1.0), PQ_M1);
    ICtCp[0] = pow(
        (PQ_C1 + PQ_C2 * E) / (1.0 + PQ_C3 * E),
        PQ_M2
    ) / MAX_LUMINANCE;
    return vec4(fromLMS * toLinear(vec4(ICtCpPQInv * ICtCp, 1.0), CM_TRANSFER_FUNCTION_ST2084_PQ).rgb * MAX_LUMINANCE, color[3]);
}

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor;
    if (texType == 1)
        pixColor = vec4(texture(tex, v_texcoord).rgb, 1.0);
//    else if (texType == 2)
//        pixColor = texture(texture0, v_texcoord);
    else // assume rgba
        pixColor = texture(tex, v_texcoord);

    if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
        discard;

    if (discardAlpha == 1 && pixColor[3] <= discardAlphaValue)
        discard;

    if (skipCM == 0) {
        pixColor = toLinear(pixColor, sourceTF);
        mat3 srcxyz = primaries2xyz(sourcePrimaries);
        mat3 dstxyz;
        if (sourcePrimaries == targetPrimaries)
            dstxyz = srcxyz;
        else {
            dstxyz = primaries2xyz(targetPrimaries);
            pixColor = convertPrimaries(pixColor, srcxyz, dstxyz);
        }
        pixColor = tonemap(pixColor, dstxyz);
        pixColor = fromLinear(pixColor, targetTF);
    }

    if (applyTint == 1)
        pixColor = vec4(pixColor.rgb * tint.rgb, pixColor[3]);

    if (radius > 0.0)
		pixColor = rounding(pixColor);
    
    fragColor = pixColor * alpha;
}
)#"
