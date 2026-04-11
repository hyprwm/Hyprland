#ifndef ROUNDING_GLSL
#define ROUNDING_GLSL
// smoothing constant for the edge: more = blurrier, but smoother
#define M_PI               3.1415926535897932384626433832795
#define SMOOTHING_CONSTANT (M_PI / 5.34665792551)

float distance(vec2 coords, float roundingPower) {
    return pow(pow(coords.x, roundingPower) + pow(coords.y, roundingPower), 1.0 / roundingPower);
}

vec4 rounding(vec4 color, float radius, float roundingPower, vec2 topLeft, vec2 fullSize) {
    vec2 pixCoord = vec2(gl_FragCoord);
    pixCoord -= topLeft + fullSize * 0.5;
    pixCoord *= vec2(lessThan(pixCoord, vec2(0.0))) * -2.0 + 1.0;
    pixCoord -= fullSize * 0.5 - radius;
    pixCoord += vec2(1.0, 1.0) / fullSize; // center the pix don't make it top-left

    if (pixCoord.x + pixCoord.y > radius) {
        float dist = distance(pixCoord, roundingPower);

        if (dist > radius + SMOOTHING_CONSTANT)
            discard;

        float normalized = 1.0 - smoothstep(0.0, 1.0, (dist - radius + SMOOTHING_CONSTANT) / (SMOOTHING_CONSTANT * 2.0));

        color *= normalized;
    }

    return color;
}
#endif