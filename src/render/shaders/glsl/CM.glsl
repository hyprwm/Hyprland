uniform vec2 srcTFRange;
uniform vec2 dstTFRange;

uniform float maxLuminance;
uniform float dstMaxLuminance;
uniform float dstRefLuminance;
uniform float sdrSaturation;
uniform float sdrBrightnessMultiplier;
uniform mat3 convertMatrix;

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
#define HLG_E_CUT (sqrt(3.0) * pow(HLG_D_CUT, 0.5))
#define HLG_A 0.17883277
#define HLG_B 0.28466892
#define HLG_C 0.55991073

#define SDR_MIN_LUMINANCE 0.2
#define SDR_MAX_LUMINANCE 80.0
#define HDR_MIN_LUMINANCE 0.005
#define HDR_MAX_LUMINANCE 10000.0
#define HLG_MAX_LUMINANCE 1000.0

#define M_E 2.718281828459045

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

// The primary source for these transfer functions is https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.1361-0-199802-W!!PDF-E.pdf
vec3 tfInvPQ(vec3 color) {
    vec3 E = pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(PQ_INV_M2));
    return pow(
        (max(E - PQ_C1, vec3(0.0))) / (PQ_C2 - PQ_C3 * E),
        vec3(PQ_INV_M1)
    );
}

vec3 tfInvHLG(vec3 color) {
    bvec3 isLow = lessThanEqual(color.rgb, vec3(HLG_D_CUT));
    vec3 lo = sqrt(3.0) * pow(color.rgb, vec3(0.5));
    vec3 hi = HLG_A * log(12.0 * color.rgb - HLG_B) + HLG_C;
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
    bvec3 isLow = lessThanEqual(color.rgb, vec3(HLG_E_CUT));
    vec3 lo = pow(color.rgb / sqrt(3.0), vec3(2.0));
    vec3 hi = (pow(vec3(M_E), (color.rgb - HLG_C) / HLG_A) + HLG_B) / 12.0;
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


mat3 adaptWhite(vec2 src, vec2 dst) {
    if (src == dst)
        return mat3(
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0
        );

    // const vec2 D65 = vec2(0.3127, 0.3290);
    const mat3 Bradford = mat3(
        0.8951, 0.2664, -0.1614,
        -0.7502, 1.7135, 0.0367,
        0.0389, -0.0685, 1.0296
    );
    mat3 BradfordInv = inverse(Bradford);
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
//const mat3 LMStoBT2020 = inverse(BT2020toLMS);
const mat3 LMStoBT2020 = mat3(
    2.0701800566956135096, -1.3264568761030210255, 0.20661600684785517081,
    0.36498825003265747974, 0.68046736285223514102, -0.045421753075853231409,
    -0.049595542238932107896, -0.049421161186757487412, 1.1879959417328034394
);

// const mat3 ICtCpPQ = transpose(mat3(
//     2048.0, 2048.0, 0.0,
//     6610.0, -13613.0, 7003.0,
//     17933.0, -17390.0, -543.0
// ) / 4096.0);
const mat3 ICtCpPQ = mat3(
    0.5,  1.61376953125,   4.378173828125,
    0.5, -3.323486328125, -4.24560546875,
    0.0,  1.709716796875, -0.132568359375
);
//const mat3 ICtCpPQInv = inverse(ICtCpPQ);
const mat3 ICtCpPQInv = mat3(
    1.0,                     1.0,                     1.0,
    0.0086090370379327566,  -0.0086090370379327566,   0.560031335710679118,
    0.11102962500302595656, -0.11102962500302595656, -0.32062717498731885185
);

// unused for now
// const mat3 ICtCpHLG = transpose(mat3(
//     2048.0, 2048.0, 0.0,
//     3625.0, -7465.0, 3840.0,
//     9500.0, -9212.0, -288.0
// ) / 4096.0);
// const mat3 ICtCpHLGInv = inverse(ICtCpHLG);

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

vec4 doColorManagement(vec4 pixColor, int srcTF, int dstTF, mat4x2 dstPrimaries) {
    pixColor.rgb /= max(pixColor.a, 0.001);
    pixColor.rgb = toLinearRGB(pixColor.rgb, srcTF);
    pixColor.rgb = convertMatrix * pixColor.rgb;
    pixColor = toNit(pixColor, srcTFRange);
    pixColor.rgb *= pixColor.a;
    mat3 dstxyz = primaries2xyz(dstPrimaries);
    pixColor = tonemap(pixColor, dstxyz);
    pixColor = fromLinearNit(pixColor, dstTF, dstTFRange);
    if (srcTF == CM_TRANSFER_FUNCTION_SRGB && dstTF == CM_TRANSFER_FUNCTION_ST2084_PQ) {
        pixColor = saturate(pixColor, dstxyz, sdrSaturation);
        pixColor.rgb *= sdrBrightnessMultiplier;
    }
    return pixColor;
}
