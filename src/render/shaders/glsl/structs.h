#ifndef STRUCTS_H
#define STRUCTS_H
struct SShaderTargetPrimaries {
    float xyz[3][3];
    float sdrSaturation;
    float sdrBrightnessMultiplier;
    float _junk;
};

struct SShaderTonemap {
    float maxLuminance;
    float dstMaxLuminance;
    float dstRefLuminance;
    float _junk;
};

struct SShaderCM {
    int   sourceTF; // eTransferFunction
    int   targetTF; // eTransferFunction
    float srcRefLuminance;

    float srcTFRange[2];
    float dstTFRange[2];

    float convertMatrix[3][3];
};

struct SRounding {
    float radius;
    float power;
    float topLeft[2];
    float fullSize[2];
    float _junk[2];
};

vec2 float2vec(float[2] v) {
    return vec2(v[0], v[1]);
}

mat3 float33TOmat3(float[3][3] mat) {
    return mat3(                         //
        mat[0][0], mat[1][0], mat[2][0], //
        mat[0][1], mat[1][1], mat[2][1], //
        mat[0][2], mat[1][2], mat[2][2]  //
    );
}
#endif
