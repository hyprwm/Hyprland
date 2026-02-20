uniform vec2 srcTFRange;
uniform vec2 dstTFRange;

uniform float srcRefLuminance;
uniform mat3 convertMatrix;

#include "sdr_mod.glsl"

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
#define SRGB_CUT 0.0031308
#define SRGB_SCALE 12.92
#define SRGB_ALPHA 1.055

#define BT1886_POW (1.0 / 0.45)
#define BT1886_CUT 0.018053968510807
#define BT1886_SCALE 4.5
#define BT1886_ALPHA (1.0 + 5.5 * BT1886_CUT)

// See http://car.france3.mars.free.fr/HD/INA-%2026%20jan%2006/SMPTE%20normes%20et%20confs/s240m.pdf
#define ST240_POW (1.0 / 0.45)
#define ST240_CUT 0.0228
#define ST240_SCALE 4.0
#define ST240_ALPHA 1.1115

#define ST428_POW 2.6
#define ST428_SCALE (52.37 / 48.0)

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
#define HLG_E_CUT 0.5
#define HLG_A 0.17883277
#define HLG_B 0.28466892
#define HLG_C 0.55991073

#define SDR_MIN_LUMINANCE 0.2
#define SDR_MAX_LUMINANCE 80.0
#define HDR_MIN_LUMINANCE 0.005
#define HDR_MAX_LUMINANCE 10000.0
#define HLG_MAX_LUMINANCE 1000.0

#define M_E 2.718281828459045

// The primary source for these transfer functions is https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.1361-0-199802-W!!PDF-E.pdf
vec3 tfInvPQ(vec3 color) {
    vec3 E = pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(PQ_INV_M2));
    return pow(
        (max(E - PQ_C1, vec3(0.0))) / (PQ_C2 - PQ_C3 * E),
        vec3(PQ_INV_M1)
    );
}

vec3 tfInvHLG(vec3 color) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(HLG_E_CUT));
    vec3 lo = color.rgb * color.rgb / 3.0;
    vec3 hi = (exp((color.rgb - HLG_C) / HLG_A) + HLG_B) / 12.0;
    return mix(hi, lo, isLow);
}

// Many transfer functions (including sRGB) follow the same pattern: a linear
// segment for small values and a power function for larger values. The
// following function implements this pattern from which sRGB, BT.1886, and
// others can be derived by plugging in the right constants.
vec3 tfInvLinPow(vec3 color, float gamma, float thres, float scale, float alpha) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(thres * scale));
    vec3 lo = color.rgb / scale;
    vec3 hi = pow((color.rgb + alpha - 1.0) / alpha, vec3(gamma));
    return mix(hi, lo, isLow);
}

vec3 tfInvSRGB(vec3 color) {
    return tfInvLinPow(color, SRGB_POW, SRGB_CUT, SRGB_SCALE, SRGB_ALPHA);
}

vec3 tfInvExtSRGB(vec3 color) {
    // EXT sRGB is the sRGB transfer function mirrored around 0.
    return sign(color) * tfInvSRGB(abs(color));
}

vec3 tfInvBT1886(vec3 color) {
    return tfInvLinPow(color, BT1886_POW, BT1886_CUT, BT1886_SCALE, BT1886_ALPHA);
}

vec3 tfInvXVYCC(vec3 color) {
    // The inverse transfer function for XVYCC is the BT1886 transfer function mirrored around 0,
    // same as what EXT sRGB is to sRGB.
    return sign(color) * tfInvBT1886(abs(color));
}

vec3 tfInvST240(vec3 color) {
    return tfInvLinPow(color, ST240_POW, ST240_CUT, ST240_SCALE, ST240_ALPHA);
}

// Forward transfer functions corresponding to the inverse functions above.
vec3 tfPQ(vec3 color) {
    vec3 E = pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(PQ_M1));
    return pow(
        (vec3(PQ_C1) + PQ_C2 * E) / (vec3(1.0) + PQ_C3 * E),
        vec3(PQ_M2)
    );
}

vec3 tfHLG(vec3 color) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(HLG_D_CUT));
    vec3 lo = sqrt(max(color.rgb, vec3(0.0)) * 3.0);
    vec3 hi = HLG_A * log(max(12.0 * color.rgb - HLG_B, vec3(0.0001))) + HLG_C;
    return mix(hi, lo, isLow);
}

vec3 tfLinPow(vec3 color, float gamma, float thres, float scale, float alpha) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(thres));
    vec3 lo = color.rgb * scale;
    vec3 hi = pow(color.rgb, vec3(1.0 / gamma)) * alpha - (alpha - 1.0);
    return mix(hi, lo, isLow);
}

vec3 tfSRGB(vec3 color) {
    return tfLinPow(color, SRGB_POW, SRGB_CUT, SRGB_SCALE, SRGB_ALPHA);
}

vec3 tfExtSRGB(vec3 color) {
    // EXT sRGB is the sRGB transfer function mirrored around 0.
    return sign(color) * tfSRGB(abs(color));
}

vec3 tfBT1886(vec3 color) {
    return tfLinPow(color, BT1886_POW, BT1886_CUT, BT1886_SCALE, BT1886_ALPHA);
}

vec3 tfXVYCC(vec3 color) {
    // The transfer function for XVYCC is the BT1886 transfer function mirrored around 0,
    // same as what EXT sRGB is to sRGB.
    return sign(color) * tfBT1886(abs(color));
}

vec3 tfST240(vec3 color) {
    return tfLinPow(color, ST240_POW, ST240_CUT, ST240_SCALE, ST240_ALPHA);
}

vec3 toLinearRGB(vec3 color, int tf) {
    switch (tf) {
        case CM_TRANSFER_FUNCTION_EXT_LINEAR:
            return color;
        case CM_TRANSFER_FUNCTION_ST2084_PQ:
            return tfInvPQ(color);
        case CM_TRANSFER_FUNCTION_GAMMA22:
            return pow(max(color, vec3(0.0)), vec3(2.2));
        case CM_TRANSFER_FUNCTION_GAMMA28:
            return pow(max(color, vec3(0.0)), vec3(2.8));
        case CM_TRANSFER_FUNCTION_HLG:
            return tfInvHLG(color);
        case CM_TRANSFER_FUNCTION_EXT_SRGB:
            return tfInvExtSRGB(color);
        case CM_TRANSFER_FUNCTION_BT1886:
            return tfInvBT1886(color);
        case CM_TRANSFER_FUNCTION_ST240:
            return tfInvST240(color);
        case CM_TRANSFER_FUNCTION_LOG_100:
            return mix(exp((color - 1.0) * 2.0 * log(10.0)), vec3(0.0), lessThanEqual(color, vec3(0.0)));
        case CM_TRANSFER_FUNCTION_LOG_316:
            return mix(exp((color - 1.0) * 2.5 * log(10.0)), vec3(0.0), lessThanEqual(color, vec3(0.0)));
        case CM_TRANSFER_FUNCTION_XVYCC:
            return tfInvXVYCC(color);
        case CM_TRANSFER_FUNCTION_ST428:
            return pow(max(color, vec3(0.0)), vec3(ST428_POW)) * ST428_SCALE;
        case CM_TRANSFER_FUNCTION_SRGB:
        default:
            return tfInvSRGB(color);
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

vec4 toNit(vec4 color, vec2 range) {
    color.rgb = color.rgb * (range[1] - range[0]) + range[0];
    return color;
}

vec3 fromLinearRGB(vec3 color, int tf) {
    switch (tf) {
        case CM_TRANSFER_FUNCTION_EXT_LINEAR:
            return color;
        case CM_TRANSFER_FUNCTION_ST2084_PQ:
            return tfPQ(color);
        case CM_TRANSFER_FUNCTION_GAMMA22:
            return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
        case CM_TRANSFER_FUNCTION_GAMMA28:
            return pow(max(color, vec3(0.0)), vec3(1.0 / 2.8));
        case CM_TRANSFER_FUNCTION_HLG:
            return tfHLG(color);
        case CM_TRANSFER_FUNCTION_EXT_SRGB:
            return tfExtSRGB(color);
        case CM_TRANSFER_FUNCTION_BT1886:
            return tfBT1886(color);
        case CM_TRANSFER_FUNCTION_ST240:
            return tfST240(color);
        case CM_TRANSFER_FUNCTION_LOG_100:
            return mix(1.0 + log(color) / log(10.0) / 2.0, vec3(0.0), lessThanEqual(color, vec3(0.01)));
        case CM_TRANSFER_FUNCTION_LOG_316:
            return mix(1.0 + log(color) / log(10.0) / 2.5, vec3(0.0), lessThanEqual(color, vec3(sqrt(10.0) / 1000.0)));
        case CM_TRANSFER_FUNCTION_XVYCC:
            return tfXVYCC(color);
        case CM_TRANSFER_FUNCTION_ST428:
            return pow(max(color, vec3(0.0)) / ST428_SCALE, vec3(1.0 / ST428_POW));
        case CM_TRANSFER_FUNCTION_SRGB:
        default:
            return tfSRGB(color);
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

vec4 fromLinearNit(vec4 color, int tf, vec2 range) {
    if (tf == CM_TRANSFER_FUNCTION_EXT_LINEAR)
        color.rgb = color.rgb / SDR_MAX_LUMINANCE;
    else {
        color.rgb /= max(color.a, 0.001);
        color.rgb = (color.rgb - range[0]) / (range[1] - range[0]);
        color.rgb = fromLinearRGB(color.rgb, tf);
        color.rgb *= color.a;
    }
    return color;
}

#include "tonemap.glsl"

vec4 doColorManagement(vec4 pixColor, int srcTF, int dstTF, mat3 dstxyz) {
    pixColor.rgb /= max(pixColor.a, 0.001);
    pixColor.rgb = toLinearRGB(pixColor.rgb, srcTF);
    pixColor.rgb = convertMatrix * pixColor.rgb;
    pixColor = toNit(pixColor, srcTFRange);
    pixColor.rgb *= pixColor.a;
    #include "do_tonemap.glsl"
    pixColor = fromLinearNit(pixColor, dstTF, dstTFRange);
    #include "do_sdr_mod.glsl"

    return pixColor;
}
