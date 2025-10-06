#version 300 es

#extension GL_ARB_shading_language_include : enable
precision highp float;
in vec4 v_color;

#include "rounding.glsl"
#include "capture.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = v_color;

    if (radius > 0.0) 
        pixColor = rounding(pixColor);

    fragColor = pixColor;
    CAPTURE_WRITE(fragColor);
}
