vec4 blur2(vec2 v_texcoord, sampler2D tex, float radius, vec2 halfpixel) {
    vec2 uv = v_texcoord / 2.0;

    vec4 sum = texture(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * radius);

    sum += texture(tex, uv + vec2(-halfpixel.x,  halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(0.0,           halfpixel.y * 2.0) * radius);
    sum += texture(tex, uv + vec2(halfpixel.x,   halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * radius);
    sum += texture(tex, uv + vec2(halfpixel.x,  -halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(0.0,          -halfpixel.y * 2.0) * radius);
    sum += texture(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;

    return sum / 12.0;
}
