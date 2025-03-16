R"#(
#version 320 es
//#extension GL_OES_EGL_image_external : require

precision highp float;
in vec2 v_texcoord;
uniform sampler2D tex;
//uniform samplerExternalOES texture0;

uniform int texType; // eTextureType: 0 - rgba, 1 - rgbx, 2 - ext
uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction
uniform mat4x2 sourcePrimaries;
uniform mat4x2 targetPrimaries;
uniform float maxLuminance;
uniform float dstMaxLuminance;
uniform float dstRefLuminance;
uniform float sdrSaturation;
uniform float sdrBrightnessMultiplier;

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

// HLG constants
#define HLG_D_CUT (1.0 / 12.0)
#define HLG_E_CUT (sqrt(3.0) * pow(HLG_D_CUT, 0.5))
#define HLG_A 0.17883277
#define HLG_B 0.28466892
#define HLG_C 0.55991073

#define SDR_MIN_LUMINANCE 0.2
#define SDR_MAX_LUMINANCE 80.0
#define HDR_MIN_LUMINANCE 0.005
#define HDR_MAX_LUMINANCE 10000.0
#define HLG_MAX_LUMINANCE 1000.0

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

vec3 xy2xyz(vec2 xy) {
    if (xy.y == 0.0)
        return vec3(0.0, 0.0, 0.0);
    
    return vec3(xy.x / xy.y, 1.0, (1.0 - xy.x - xy.y) / xy.y);
}

vec4 saturate(vec4 color, mat3 primaries, float saturation) {
    if (saturation == 1.0)
        return color;
    vec3 brightness = vec3(primaries[1][0], primaries[1][1], primaries[1][2]);
    float Y = dot(color.rgb, brightness);
    return vec4(mix(vec3(Y), color.rgb, saturation), color[3]);
}

vec3 toLinearRGB(vec3 color, int tf) {
    if (tf == CM_TRANSFER_FUNCTION_EXT_LINEAR)
        return color;
    
    bvec3 isLow;
    vec3 lo;
    vec3 hi;
    switch (tf) {
        case CM_TRANSFER_FUNCTION_ST2084_PQ:
            vec3 E = pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(PQ_INV_M2));
            return pow(
                (max(E - PQ_C1, vec3(0.0))) / (PQ_C2 - PQ_C3 * E),
                vec3(PQ_INV_M1)
            );
        case CM_TRANSFER_FUNCTION_GAMMA22:
            return pow(max(color.rgb, vec3(0.0)), vec3(2.2));
        case CM_TRANSFER_FUNCTION_GAMMA28:
            return pow(max(color.rgb, vec3(0.0)), vec3(2.8));
        case CM_TRANSFER_FUNCTION_HLG:
            isLow = lessThanEqual(color.rgb, vec3(HLG_D_CUT));
            lo = sqrt(3.0) * pow(color.rgb, vec3(0.5));
            hi = HLG_A * log(12.0 * color.rgb - HLG_B) + HLG_C;
            return mix(hi, lo, isLow);
        case CM_TRANSFER_FUNCTION_BT1886:
        case CM_TRANSFER_FUNCTION_ST240:
        case CM_TRANSFER_FUNCTION_LOG_100:
        case CM_TRANSFER_FUNCTION_LOG_316:
        case CM_TRANSFER_FUNCTION_XVYCC:
        case CM_TRANSFER_FUNCTION_EXT_SRGB:
        case CM_TRANSFER_FUNCTION_ST428:

        case CM_TRANSFER_FUNCTION_SRGB:
        default:
            isLow = lessThanEqual(color.rgb, vec3(SRGB_D_CUT));
            lo = color.rgb / SRGB_LO;
            hi = pow((color.rgb + SRGB_HI_ADD) / SRGB_HI, vec3(SRGB_POW));
            return mix(hi, lo, isLow);
    }
}

vec4 toLinear(vec4 color, int tf) {
    if (tf == CM_TRANSFER_FUNCTION_EXT_LINEAR)
        return color;

    color.rgb /= max(color.a, 0.001);
    color.rgb = toLinearRGB(color.rgb, tf);
    color.rgb *= color.a;
    return color;
}

vec4 toNit(vec4 color, int tf) {
    if (tf == CM_TRANSFER_FUNCTION_EXT_LINEAR)
        color.rgb = color.rgb * SDR_MAX_LUMINANCE;
    else {

        switch (tf) {
            case CM_TRANSFER_FUNCTION_ST2084_PQ:
                color.rgb = color.rgb * (HDR_MAX_LUMINANCE - HDR_MIN_LUMINANCE) + HDR_MIN_LUMINANCE; break;
            case CM_TRANSFER_FUNCTION_HLG:
                color.rgb = color.rgb * (HLG_MAX_LUMINANCE - HDR_MIN_LUMINANCE) + HDR_MIN_LUMINANCE; break;
            case CM_TRANSFER_FUNCTION_GAMMA22:
            case CM_TRANSFER_FUNCTION_GAMMA28:
            case CM_TRANSFER_FUNCTION_BT1886:
            case CM_TRANSFER_FUNCTION_ST240:
            case CM_TRANSFER_FUNCTION_LOG_100:
            case CM_TRANSFER_FUNCTION_LOG_316:
            case CM_TRANSFER_FUNCTION_XVYCC:
            case CM_TRANSFER_FUNCTION_EXT_SRGB:
            case CM_TRANSFER_FUNCTION_ST428:
            case CM_TRANSFER_FUNCTION_SRGB:
            default:
                color.rgb = color.rgb * (SDR_MAX_LUMINANCE - SDR_MIN_LUMINANCE) + SDR_MIN_LUMINANCE; break;
        }
    }
    return color;
}

vec3 fromLinearRGB(vec3 color, int tf) {
    bvec3 isLow;
    vec3 lo;
    vec3 hi;
    
    switch (tf) {
        case CM_TRANSFER_FUNCTION_EXT_LINEAR:
            return color;
        case CM_TRANSFER_FUNCTION_ST2084_PQ:
            vec3 E = pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(PQ_M1));
            return pow(
                (vec3(PQ_C1) + PQ_C2 * E) / (vec3(1.0) + PQ_C3 * E),
                vec3(PQ_M2)
            );
            break;
        case CM_TRANSFER_FUNCTION_GAMMA22:
            return pow(max(color.rgb, vec3(0.0)), vec3(1.0 / 2.2));
        case CM_TRANSFER_FUNCTION_GAMMA28:
            return pow(max(color.rgb, vec3(0.0)), vec3(1.0 / 2.8));
        case CM_TRANSFER_FUNCTION_HLG:
            isLow = lessThanEqual(color.rgb, vec3(HLG_E_CUT));
            lo = pow(color.rgb / sqrt(3.0), vec3(2.0));
            hi = (pow(vec3(M_E), (color.rgb - HLG_C) / HLG_A) + HLG_B) / 12.0;
            return mix(hi, lo, isLow);
        case CM_TRANSFER_FUNCTION_BT1886:
        case CM_TRANSFER_FUNCTION_ST240:
        case CM_TRANSFER_FUNCTION_LOG_100:
        case CM_TRANSFER_FUNCTION_LOG_316:
        case CM_TRANSFER_FUNCTION_XVYCC:
        case CM_TRANSFER_FUNCTION_EXT_SRGB:
        case CM_TRANSFER_FUNCTION_ST428:

        case CM_TRANSFER_FUNCTION_SRGB:
        default:
            isLow = lessThanEqual(color.rgb, vec3(SRGB_E_CUT));
            lo = color.rgb * SRGB_LO;
            hi = pow(color.rgb, vec3(SRGB_INV_POW)) * SRGB_HI - SRGB_HI_ADD;
            return mix(hi, lo, isLow);
    }
}

vec4 fromLinear(vec4 color, int tf) {
    if (tf == CM_TRANSFER_FUNCTION_EXT_LINEAR)
        return color;
    
    color.rgb /= max(color.a, 0.001);
    color.rgb = fromLinearRGB(color.rgb, tf);
    color.rgb *= color.a;
    return color;
}

vec4 fromLinearNit(vec4 color, int tf) {
    if (tf == CM_TRANSFER_FUNCTION_EXT_LINEAR)
        color.rgb = color.rgb / SDR_MAX_LUMINANCE;
    else {
        color.rgb /= max(color.a, 0.001);
        
        switch (tf) {
            case CM_TRANSFER_FUNCTION_ST2084_PQ:
                color.rgb = (color.rgb - HDR_MIN_LUMINANCE) / (HDR_MAX_LUMINANCE - HDR_MIN_LUMINANCE); break;
            case CM_TRANSFER_FUNCTION_HLG:
                color.rgb = (color.rgb - HDR_MIN_LUMINANCE) / (HLG_MAX_LUMINANCE - HDR_MIN_LUMINANCE); break;
            case CM_TRANSFER_FUNCTION_GAMMA22:
            case CM_TRANSFER_FUNCTION_GAMMA28:
            case CM_TRANSFER_FUNCTION_BT1886:
            case CM_TRANSFER_FUNCTION_ST240:
            case CM_TRANSFER_FUNCTION_LOG_100:
            case CM_TRANSFER_FUNCTION_LOG_316:
            case CM_TRANSFER_FUNCTION_XVYCC:
            case CM_TRANSFER_FUNCTION_EXT_SRGB:
            case CM_TRANSFER_FUNCTION_ST428:
            case CM_TRANSFER_FUNCTION_SRGB:
            default:
                color.rgb = (color.rgb - SDR_MIN_LUMINANCE) / (SDR_MAX_LUMINANCE - SDR_MIN_LUMINANCE); break;
        }

        color.rgb = fromLinearRGB(color.rgb, tf);
        
        color.rgb *= color.a;
    }
    return color;
}

mat3 primaries2xyz(mat4x2 primaries) {
    vec3 r = xy2xyz(primaries[0]);
    vec3 g = xy2xyz(primaries[1]);
    vec3 b = xy2xyz(primaries[2]);
    vec3 w = xy2xyz(primaries[3]);
    
    mat3 invMat = inverse(
       mat3(
            r.x, r.y, r.z,
            g.x, g.y, g.z,
            b.x, b.y, b.z
        )
    );

    vec3 s = invMat * w;

    return mat3(
        s.r * r.x, s.r * r.y, s.r * r.z,
        s.g * g.x, s.g * g.y, s.g * g.z,
        s.b * b.x, s.b * b.y, s.b * b.z
    );
}

// const vec2 D65 = vec2(0.3127, 0.3290);
const mat3 Bradford = mat3(
    0.8951, 0.2664, -0.1614,
    -0.7502, 1.7135, 0.0367,
    0.0389, -0.0685, 1.0296
);
const mat3 BradfordInv = inverse(Bradford);

mat3 adaptWhite(vec2 src, vec2 dst) {
    if (src == dst)
        return mat3(
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0
        );

    vec3 srcXYZ = xy2xyz(src);
    vec3 dstXYZ = xy2xyz(dst);
    vec3 factors = (Bradford * dstXYZ) / (Bradford * srcXYZ);

    return BradfordInv * mat3(
        factors.x, 0.0, 0.0,
        0.0, factors.y, 0.0,
        0.0, 0.0, factors.z
    ) * Bradford;
}

vec4 convertPrimaries(vec4 color, mat3 src, vec2 srcWhite, mat3 dst, vec2 dstWhite) {
    mat3 convMat = inverse(dst) * adaptWhite(srcWhite, dstWhite) * src;
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

    vec3 lms = fromLinear(vec4((toLMS * color.rgb) / HDR_MAX_LUMINANCE, 1.0), CM_TRANSFER_FUNCTION_ST2084_PQ).rgb;
    vec3 ICtCp = ICtCpPQ * lms;

    float E = pow(clamp(ICtCp[0], 0.0, 1.0), PQ_INV_M2);
    float luminance = pow(
        (max(E - PQ_C1, 0.0)) / (PQ_C2 - PQ_C3 * E),
        PQ_INV_M1
    ) * HDR_MAX_LUMINANCE;

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
    ) / HDR_MAX_LUMINANCE;
    return vec4(fromLMS * toLinear(vec4(ICtCpPQInv * ICtCp, 1.0), CM_TRANSFER_FUNCTION_ST2084_PQ).rgb * HDR_MAX_LUMINANCE, color[3]);
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

    pixColor.rgb /= max(pixColor.a, 0.001);
    pixColor.rgb = toLinearRGB(pixColor.rgb, sourceTF);
    mat3 srcxyz = primaries2xyz(sourcePrimaries);
    mat3 dstxyz;

    if (sourcePrimaries == targetPrimaries)
        dstxyz = srcxyz;
    else {
        dstxyz = primaries2xyz(targetPrimaries);
        pixColor = convertPrimaries(pixColor, srcxyz, sourcePrimaries[3], dstxyz, targetPrimaries[3]);
    }

    pixColor = toNit(pixColor, sourceTF);
    pixColor.rgb *= pixColor.a;
    pixColor = tonemap(pixColor, dstxyz);

    if (sourceTF == CM_TRANSFER_FUNCTION_SRGB && targetTF == CM_TRANSFER_FUNCTION_ST2084_PQ)
        pixColor = saturate(pixColor, srcxyz, sdrSaturation);

    pixColor *= sdrBrightnessMultiplier;
    pixColor = fromLinearNit(pixColor, targetTF);

    if (applyTint == 1)
        pixColor = vec4(pixColor.rgb * tint.rgb, pixColor[3]);

    if (radius > 0.0)
        pixColor = rounding(pixColor);
    
    fragColor = pixColor * alpha;
}
)#"
