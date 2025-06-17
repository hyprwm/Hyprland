#version 320 es

uniform mat3 proj;
uniform vec4 color;
uniform bool instancedDraw;

in vec2 pos;
in vec2 texcoord;
in vec2 texcoordMatte;
in vec4 instanceRect;

out vec4 v_color;
out vec2 v_texcoord;
out vec2 v_texcoordMatte;
out vec4 v_clipRect;

void main() {
    vec2 finalPos;

    if (instancedDraw) {
        vec2 halfSize = instanceRect.zw * 0.5;
        vec2 center = instanceRect.xy + halfSize;
        finalPos = center + pos * halfSize;
    } else {
        finalPos = pos;
    }

    vec3 worldPos = proj * vec3(finalPos, 1.0);
    gl_Position = vec4(worldPos, 1.0);

    v_color = color;
    v_texcoord = texcoord;
    v_texcoordMatte = texcoordMatte;
}
