#include "Math.hpp"
#include <unordered_map>
#include <cstring>

Hyprutils::Math::eTransform wlTransformToHyprutils(wl_output_transform t) {
    switch (t) {
        case WL_OUTPUT_TRANSFORM_NORMAL: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_NORMAL;
        case WL_OUTPUT_TRANSFORM_180: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_180;
        case WL_OUTPUT_TRANSFORM_90: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_90;
        case WL_OUTPUT_TRANSFORM_270: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_270;
        case WL_OUTPUT_TRANSFORM_FLIPPED: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_270;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90: return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_90;
        default: break;
    }
    return Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_NORMAL;
}

void matrixIdentity(float mat[9]) {
    static const float identity[9] = {
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    memcpy(mat, identity, sizeof(identity));
}

void matrixMultiply(float mat[9], const float a[9], const float b[9]) {
    float product[9];

    product[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
    product[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
    product[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];

    product[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
    product[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
    product[5] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];

    product[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
    product[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
    product[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];

    memcpy(mat, product, sizeof(product));
}

void matrixTranspose(float mat[9], const float a[9]) {
    float transposition[9] = {
        a[0], a[3], a[6], a[1], a[4], a[7], a[2], a[5], a[8],
    };
    memcpy(mat, transposition, sizeof(transposition));
}

void matrixTranslate(float mat[9], float x, float y) {
    float translate[9] = {
        1.0f, 0.0f, x, 0.0f, 1.0f, y, 0.0f, 0.0f, 1.0f,
    };
    matrixMultiply(mat, mat, translate);
}

void matrixScale(float mat[9], float x, float y) {
    float scale[9] = {
        x, 0.0f, 0.0f, 0.0f, y, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    matrixMultiply(mat, mat, scale);
}

void matrixRotate(float mat[9], float rad) {
    float rotate[9] = {
        cos(rad), -sin(rad), 0.0f, sin(rad), cos(rad), 0.0f, 0.0f, 0.0f, 1.0f,
    };
    matrixMultiply(mat, mat, rotate);
}

std::unordered_map<eTransform, std::array<float, 9>> transforms = {
    {HYPRUTILS_TRANSFORM_NORMAL,
     {
         1.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
     }},
    {HYPRUTILS_TRANSFORM_90,
     {
         0.0f,
         1.0f,
         0.0f,
         -1.0f,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
     }},
    {HYPRUTILS_TRANSFORM_180,
     {
         -1.0f,
         0.0f,
         0.0f,
         0.0f,
         -1.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
     }},
    {HYPRUTILS_TRANSFORM_270,
     {
         0.0f,
         -1.0f,
         0.0f,
         1.0f,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
     }},
    {HYPRUTILS_TRANSFORM_FLIPPED,
     {
         -1.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
     }},
    {HYPRUTILS_TRANSFORM_FLIPPED_90,
     {
         0.0f,
         1.0f,
         0.0f,
         1.0f,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
     }},
    {HYPRUTILS_TRANSFORM_FLIPPED_180,
     {
         1.0f,
         0.0f,
         0.0f,
         0.0f,
         -1.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
     }},
    {HYPRUTILS_TRANSFORM_FLIPPED_270,
     {
         0.0f,
         -1.0f,
         0.0f,
         -1.0f,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
     }},
};

void matrixTransform(float mat[9], eTransform transform) {
    matrixMultiply(mat, mat, transforms.at(transform).data());
}

void matrixProjection(float mat[9], int width, int height, eTransform transform) {
    memset(mat, 0, sizeof(*mat) * 9);

    const float* t = transforms.at(transform).data();
    float        x = 2.0f / width;
    float        y = 2.0f / height;

    // Rotation + reflection
    mat[0] = x * t[0];
    mat[1] = x * t[1];
    mat[3] = y * -t[3];
    mat[4] = y * -t[4];

    // Translation
    mat[2] = -copysign(1.0f, mat[0] + mat[1]);
    mat[5] = -copysign(1.0f, mat[3] + mat[4]);

    // Identity
    mat[8] = 1.0f;
}

void projectBox(float mat[9], CBox& box, eTransform transform, float rotation, const float projection[9]) {
    double x      = box.x;
    double y      = box.y;
    double width  = box.width;
    double height = box.height;

    matrixIdentity(mat);
    matrixTranslate(mat, x, y);

    if (rotation != 0) {
        matrixTranslate(mat, width / 2, height / 2);
        matrixRotate(mat, rotation);
        matrixTranslate(mat, -width / 2, -height / 2);
    }

    matrixScale(mat, width, height);

    if (transform != HYPRUTILS_TRANSFORM_NORMAL) {
        matrixTranslate(mat, 0.5, 0.5);
        matrixTransform(mat, transform);
        matrixTranslate(mat, -0.5, -0.5);
    }

    matrixMultiply(mat, projection, mat);
}

wl_output_transform invertTransform(wl_output_transform tr) {
    if ((tr & WL_OUTPUT_TRANSFORM_90) && !(tr & WL_OUTPUT_TRANSFORM_FLIPPED))
        tr = (wl_output_transform)(tr ^ (int)WL_OUTPUT_TRANSFORM_180);

    return tr;
}
