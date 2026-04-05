vec4[2] openAnimation(highp sampler2D tex, vec2 coords, vec2 size, float progress, float randomSeed) {
    float mod = clamp(1.0 - (coords.x + coords.y) / 2.0 + progress, 0.0, 1.0);
    vec4 color = texture(tex, coords);
    color.a *= mod;
    vec4[2] result;
    result[0] = color;
    result[1].xy = coords;
    result[1].a = mod;
    return result;
}

vec4[2] closeAnimation(highp sampler2D tex, vec2 coords, vec2 size, float progress, float randomSeed) {
    float mod = clamp((coords.x + coords.y) / 2.0 + progress, 0.0, 1.0);
    vec4 color = texture(tex, coords);
    color.a *= mod;
    vec4[2] result;
    result[0] = color;
    result[1].xy = coords;
    result[1].a = mod;
    return result;
}
