uniform float maxLuminance;
uniform float dstMaxLuminance;
uniform float dstRefLuminance;

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

    float linearPart = min(luminance, dstRefLuminance);
    float luminanceAboveRef = max(luminance - dstRefLuminance, 0.0);
    float maxExcessLuminance = max(maxLuminance - dstRefLuminance, 1.0);
    float shoulder = log((luminanceAboveRef / maxExcessLuminance + 1.0) * (M_E - 1.0));
    float mappedHigh = shoulder * (dstMaxLuminance - dstRefLuminance);
    float newLum = clamp(linearPart + mappedHigh, 0.0, dstMaxLuminance);

    // scale src to dst reference
    float refScale = dstRefLuminance / srcRefLuminance;

    return vec4(fromLMS * toLinear(vec4(ICtCpPQInv * ICtCp, 1.0), CM_TRANSFER_FUNCTION_ST2084_PQ).rgb * HDR_MAX_LUMINANCE * refScale, color[3]); 
}
