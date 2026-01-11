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
    if (gradientLength < 2)
        return gradient[0];

    float finalAng = 0.0;

    if (angle > 4.71 /* 270 deg */) {
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = 6.28 - angle;
    } else if (angle > 3.14 /* 180 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = angle - 3.14;
    } else if (angle > 1.57 /* 90 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        finalAng = 3.14 - angle;
    } else {
        finalAng = angle;
    }

    float sine = sin(finalAng);

    float progress = (normalizedCoord[1] * sine + normalizedCoord[0] * (1.0 - sine)) * float(gradientLength - 1);
    int bottom = int(floor(progress));
    int top = bottom + 1;

    return gradient[top] * (progress - float(bottom)) + gradient[bottom] * (float(top) - progress);
}

vec4 getOkColorForCoordArray2(vec2 normalizedCoord) {
    if (gradient2Length < 2)
        return gradient2[0];

    float finalAng = 0.0;

    if (angle2 > 4.71 /* 270 deg */) {
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = 6.28 - angle;
    } else if (angle2 > 3.14 /* 180 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        normalizedCoord[1] = 1.0 - normalizedCoord[1];
        finalAng = angle - 3.14;
    } else if (angle2 > 1.57 /* 90 deg */) {
        normalizedCoord[0] = 1.0 - normalizedCoord[0];
        finalAng = 3.14 - angle2;
    } else {
        finalAng = angle2;
    }

    float sine = sin(finalAng);

    float progress = (normalizedCoord[1] * sine + normalizedCoord[0] * (1.0 - sine)) * float(gradient2Length - 1);
    int bottom = int(floor(progress));
    int top = bottom + 1;

    return gradient2[top] * (progress - float(bottom)) + gradient2[bottom] * (float(top) - progress);
}

vec4 getColorForCoord(vec2 normalizedCoord) {
    vec4 result1 = getOkColorForCoordArray1(normalizedCoord);

    if (gradient2Length <= 0)
        return okLabAToSrgb(result1);

    vec4 result2 = getOkColorForCoordArray2(normalizedCoord);

    return okLabAToSrgb(mix(result1, result2, gradientLerp));
}
