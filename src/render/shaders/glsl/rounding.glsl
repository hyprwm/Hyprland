#ifndef ROUNDING_GLSL
#define ROUNDING_GLSL
// smoothing constant for the edge: more = blurrier, but smoother
#define M_PI               3.1415926535897932384626433832795
#define SMOOTHING_CONSTANT (M_PI / 5.34665792551)

float distanceWithRounding(vec2 coords, float roundingPower) {
    return pow(pow(coords.x, roundingPower) + pow(coords.y, roundingPower), 1.0 / roundingPower);
}

float cornerRadius(vec4 radii, vec2 position) {
    vec2 row = mix(radii.xy, radii.wz, step(0.0, position.y));
    return mix(row.x, row.y, step(0.0, position.x));
}

vec4 rounding(vec4 color, vec4 radii, float roundingPower, vec2 topLeft, vec2 fullSize) {
    vec2 pixCoord = vec2(gl_FragCoord);
    pixCoord -= topLeft + fullSize * 0.5;
    float radius = cornerRadius(radii, pixCoord);
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoord -= fullSize * 0.5 - radius;
    pixCoord += vec2(1.0, 1.0) / fullSize; // center the pix don't make it top-left

    if (radius > 0.0 && pixCoord.x + pixCoord.y > radius) {
        float dist = distanceWithRounding(pixCoord, roundingPower);

        if (dist > radius + SMOOTHING_CONSTANT)
            discard;

        float normalized = 1.0 - smoothstep(0.0, 1.0, (dist - radius + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));

        color *= normalized;
    }

    return color;
}
#endif
