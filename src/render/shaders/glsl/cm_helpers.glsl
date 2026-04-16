#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif
#ifndef CM_HELPERS_GLSL
#define CM_HELPERS_GLSL

#include "defines.h"
#include "constants.h"

#if USE_SDR_MOD
vec4 saturate(vec4 color, mat3 primaries, float saturation) {
    if (saturation == 1.0)
        return color;
    vec3  brightness = vec3(primaries[1][0], primaries[1][1], primaries[1][2]);
    float Y          = dot(color.rgb, brightness);
    return vec4(mix(vec3(Y), color.rgb, saturation), color[3]);
}
#endif

vec3 applyIcc3DLut(vec3 linearRgb01, highp sampler3D iccLut3D, float iccLutSize) {
    vec3 x = clamp(linearRgb01, 0.0, 1.0);

    // Map [0..1] to texel centers to avoid edge issues
    float N     = iccLutSize;
    vec3  coord = (x * (N - 1.0) + 0.5) / N;

    return texture(iccLut3D, coord).rgb;
}

vec3 xy2xyz(vec2 xy) {
    if (xy.y == 0.0)
        return vec3(0.0, 0.0, 0.0);

    return vec3(xy.x / xy.y, 1.0, (1.0 - xy.x - xy.y) / xy.y);
}

// The primary source for these transfer functions is https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.1361-0-199802-W!!PDF-E.pdf
vec3 tfInvPQ(vec3 color) {
    vec3 E = pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(PQ_INV_M2));
    return pow((max(E - PQ_C1, vec3(0.0))) / (PQ_C2 - PQ_C3 * E), vec3(PQ_INV_M1));
}

vec3 tfInvHLG(vec3 color) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(HLG_E_CUT));
    vec3  lo    = color.rgb * color.rgb / 3.0;
    vec3  hi    = (exp((color.rgb - HLG_C) / HLG_A) + HLG_B) / 12.0;
    return mix(hi, lo, isLow);
}

// Many transfer functions (including sRGB) follow the same pattern: a linear
// segment for small values and a power function for larger values. The
// following function implements this pattern from which sRGB, BT.1886, and
// others can be derived by plugging in the right constants.
vec3 tfInvLinPow(vec3 color, float gamma, float thres, float scale, float alpha) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(thres * scale));
    vec3  lo    = color.rgb / scale;
    vec3  hi    = pow((color.rgb + alpha - 1.0) / alpha, vec3(gamma));
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
    return pow((vec3(PQ_C1) + PQ_C2 * E) / (vec3(1.0) + PQ_C3 * E), vec3(PQ_M2));
}

vec3 tfHLG(vec3 color) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(HLG_D_CUT));
    vec3  lo    = sqrt(max(color.rgb, vec3(0.0)) * 3.0);
    vec3  hi    = HLG_A * log(max(12.0 * color.rgb - HLG_B, vec3(0.0001))) + HLG_C;
    return mix(hi, lo, isLow);
}

vec3 tfLinPow(vec3 color, float gamma, float thres, float scale, float alpha) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(thres));
    vec3  lo    = color.rgb * scale;
    vec3  hi    = pow(color.rgb, vec3(1.0 / gamma)) * alpha - (alpha - 1.0);
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
        case CM_TRANSFER_FUNCTION_EXT_LINEAR: return color;
        case CM_TRANSFER_FUNCTION_ST2084_PQ: return tfInvPQ(color);
        case CM_TRANSFER_FUNCTION_GAMMA22: return pow(max(color, vec3(0.0)), vec3(2.2));
        case CM_TRANSFER_FUNCTION_GAMMA28: return pow(max(color, vec3(0.0)), vec3(2.8));
        case CM_TRANSFER_FUNCTION_HLG: return tfInvHLG(color);
        case CM_TRANSFER_FUNCTION_EXT_SRGB: return tfInvExtSRGB(color);
        case CM_TRANSFER_FUNCTION_BT1886: return tfInvBT1886(color);
        case CM_TRANSFER_FUNCTION_ST240: return tfInvST240(color);
        case CM_TRANSFER_FUNCTION_LOG_100: return mix(exp((color - 1.0) * 2.0 * log(10.0)), vec3(0.0), lessThanEqual(color, vec3(0.0)));
        case CM_TRANSFER_FUNCTION_LOG_316: return mix(exp((color - 1.0) * 2.5 * log(10.0)), vec3(0.0), lessThanEqual(color, vec3(0.0)));
        case CM_TRANSFER_FUNCTION_XVYCC: return tfInvXVYCC(color);
        case CM_TRANSFER_FUNCTION_ST428: return pow(max(color, vec3(0.0)), vec3(ST428_POW)) * ST428_SCALE;
        case CM_TRANSFER_FUNCTION_SRGB:
        default: return tfInvSRGB(color);
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
        case CM_TRANSFER_FUNCTION_EXT_LINEAR: return color;
        case CM_TRANSFER_FUNCTION_ST2084_PQ: return tfPQ(color);
        case CM_TRANSFER_FUNCTION_GAMMA22: return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
        case CM_TRANSFER_FUNCTION_GAMMA28: return pow(max(color, vec3(0.0)), vec3(1.0 / 2.8));
        case CM_TRANSFER_FUNCTION_HLG: return tfHLG(color);
        case CM_TRANSFER_FUNCTION_EXT_SRGB: return tfExtSRGB(color);
        case CM_TRANSFER_FUNCTION_BT1886: return tfBT1886(color);
        case CM_TRANSFER_FUNCTION_ST240: return tfST240(color);
        case CM_TRANSFER_FUNCTION_LOG_100: return mix(1.0 + log(color) / log(10.0) / 2.0, vec3(0.0), lessThanEqual(color, vec3(0.01)));
        case CM_TRANSFER_FUNCTION_LOG_316: return mix(1.0 + log(color) / log(10.0) / 2.5, vec3(0.0), lessThanEqual(color, vec3(sqrt(10.0) / 1000.0)));
        case CM_TRANSFER_FUNCTION_XVYCC: return tfXVYCC(color);
        case CM_TRANSFER_FUNCTION_ST428: return pow(max(color, vec3(0.0)) / ST428_SCALE, vec3(1.0 / ST428_POW));
        case CM_TRANSFER_FUNCTION_SRGB:
        default: return tfSRGB(color);
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
    color.rgb = (color.rgb - range[0] * color.a) / (range[1] - range[0]); // @gulafaran
    color.rgb /= max(color.a, 0.001);
    color.rgb = fromLinearRGB(color.rgb, tf);
    color.rgb *= color.a;
    return color;
}

#if USE_TONEMAP
#include "tonemap.glsl"
#endif

#if USE_MIRROR
vec4[2]
#else
vec4
#endif
    doColorManagement(vec4 pixColor, int srcTF, int dstTF, mat3 convertMatrix, vec2 srcTFRange, vec2 dstTFRange
#if USE_ICC
                      ,
                      highp sampler3D iccLut3D, float iccLutSize
#else
#if USE_TONEMAP || USE_SDR_MOD
                       ,
                       mat3 dstxyz
#endif
#if USE_TONEMAP
                       ,
                       float maxLuminance, float dstMaxLuminance, float dstRefLuminance, float srcRefLuminance
#endif
#if USE_SDR_MOD
                       ,
                       float sdrSaturation, float sdrBrightnessMultiplier
#endif
#endif
    ) {
    pixColor.rgb /= max(pixColor.a, 0.001);
    pixColor.rgb = toLinearRGB(pixColor.rgb, srcTF);
#if USE_ICC
    pixColor.rgb = applyIcc3DLut(pixColor.rgb, iccLut3D, iccLutSize);
    pixColor.rgb *= pixColor.a;
#else
    pixColor.rgb = convertMatrix * pixColor.rgb;
    pixColor     = toNit(pixColor, srcTFRange);
    pixColor.rgb *= pixColor.a;
#if USE_TONEMAP
    pixColor = tonemap(pixColor, dstxyz, maxLuminance, dstMaxLuminance, dstRefLuminance, srcRefLuminance);
#endif
#if USE_MIRROR
    // TODO HDR -> SDR tonemap
    vec4 mirrorColor = fromLinearNit(pixColor, CM_TRANSFER_FUNCTION_SRGB,
                                     srcTF == CM_TRANSFER_FUNCTION_GAMMA22 || srcTF == CM_TRANSFER_FUNCTION_SRGB ? srcTFRange : vec2(SDR_MIN_LUMINANCE, SDR_MAX_LUMINANCE));
#endif
    pixColor = fromLinearNit(pixColor, dstTF, dstTFRange);
#if USE_SDR_MOD
    pixColor = saturate(pixColor, dstxyz, sdrSaturation);
    pixColor.rgb *= sdrBrightnessMultiplier;
#endif
#endif

#if USE_MIRROR
    vec4[2] pixColors;
    pixColors[0] = pixColor;
    pixColors[1] = mirrorColor;
    return pixColors;
#else
    return pixColor;
#endif
}

#endif