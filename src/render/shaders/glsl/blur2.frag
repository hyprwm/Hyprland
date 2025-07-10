#version 300 es
precision highp float;

uniform sampler2D tex;
uniform float radius;
uniform vec2 halfpixel;

in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() {
    vec2 uv = v_texcoord / 2.0;

    vec4 sum = texture(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * radius);

    sum += texture(tex, uv + vec2(-halfpixel.x,  halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(0.0,           halfpixel.y * 2.0) * radius);
    sum += texture(tex, uv + vec2(halfpixel.x,   halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * radius);
    sum += texture(tex, uv + vec2(halfpixel.x,  -halfpixel.y) * radius) * 2.0;
    sum += texture(tex, uv + vec2(0.0,          -halfpixel.y * 2.0) * radius);
    sum += texture(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;

    fragColor = sum / 12.0;
}
