#extension GL_ARB_shading_language_include : enable
precision highp float;
varying vec4 v_color;

#include "rounding.glsl"

void main() {
    vec4 pixColor = v_color;

    if (radius > 0.0) 
        pixColor = rounding(pixColor);

    gl_FragColor = pixColor;
}
