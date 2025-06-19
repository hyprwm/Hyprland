#version 300 es

precision highp float;
in vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;
uniform sampler2D texMatte;

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = texture2D(tex, v_texcoord) * texture2D(texMatte, v_texcoord)[0]; // I know it only uses R, but matte should be black/white anyways.
}
