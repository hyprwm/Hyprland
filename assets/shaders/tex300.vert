#version 300 es

uniform mat3 proj;
uniform vec4 color;
in vec2 pos;
in vec2 texcoord;
in vec2 texcoordMatte;
out vec4 v_color;
out vec2 v_texcoord;
out vec2 v_texcoordMatte;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_color = color;
    v_texcoord = texcoord;
    v_texcoordMatte = texcoordMatte;
}
