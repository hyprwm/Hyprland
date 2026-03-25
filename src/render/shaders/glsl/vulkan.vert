#version 450

layout(push_constant, row_major) uniform UBO {
    mat4 proj;
    vec2 uvOffset;
    vec2 uvSize;
}
data;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec2 uvOffset;
layout(location = 2) out vec2 uvSize;

void main() {
    vec2 pos = vec2(float((gl_VertexIndex + 1) & 2) * 0.5f, float(gl_VertexIndex & 2) * 0.5f);
    // uv = data.uvOffset + pos * data.uvSize;
    uv          = pos;
    uvOffset    = data.uvOffset;
    uvSize      = data.uvSize;
    gl_Position = data.proj * vec4(pos, 0.0, 1.0);
}
