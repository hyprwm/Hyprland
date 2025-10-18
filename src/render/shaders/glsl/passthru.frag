#version 300 es

precision highp float;
in vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

#include "capture.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = texture(tex, v_texcoord);
    CAPTURE_WRITE(fragColor);
}
