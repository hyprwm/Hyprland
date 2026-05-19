#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = texture(tex, v_texcoord);
}
