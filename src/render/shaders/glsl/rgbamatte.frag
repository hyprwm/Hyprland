#version 300 es

precision highp float;
#include "capture.glsl"
in vec2           v_texcoord; // is in 0-1
uniform sampler2D tex;
uniform sampler2D texMatte;

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 outColor = texture(tex, v_texcoord) * texture(texMatte, v_texcoord)[0]; // I know it only uses R, but matte should be black/white anyways.
    fragColor     = outColor;
    CAPTURE_WRITE(outColor);
}
