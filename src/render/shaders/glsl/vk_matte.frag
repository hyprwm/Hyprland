#version 450

precision highp float;
layout(location = 0) in vec2 v_texcoord;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 1, binding = 0) uniform sampler2D texMatte;

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = texture(tex, v_texcoord) * texture(texMatte, v_texcoord)[0]; // I know it only uses R, but matte should be black/white anyways.
}
