#version 320 es
//#extension GL_OES_EGL_image_external : require
#extension GL_ARB_shading_language_include : enable

precision highp float;
in vec2 v_texcoord;
uniform sampler2D tex;
//uniform samplerExternalOES texture0;

uniform int texType; // eTextureType: 0 - rgba, 1 - rgbx, 2 - ext
// uniform int skipCM;
uniform int sourceTF; // eTransferFunction
uniform int targetTF; // eTransferFunction
uniform mat4x2 sourcePrimaries;
uniform mat4x2 targetPrimaries;

uniform float alpha;

uniform int discardOpaque;
uniform int discardAlpha;
uniform float discardAlphaValue;

uniform int applyTint;
uniform vec3 tint;

#include "rounding.glsl"
#include "CM.glsl"

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor;
    if (texType == 1)
        pixColor = vec4(texture(tex, v_texcoord).rgb, 1.0);
//    else if (texType == 2)
//        pixColor = texture(texture0, v_texcoord);
    else // assume rgba
        pixColor = texture(tex, v_texcoord);

    if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
        discard;

    if (discardAlpha == 1 && pixColor[3] <= discardAlphaValue)
        discard;

    // this shader shouldn't be used when skipCM == 1
    pixColor = doColorManagement(pixColor, sourceTF, sourcePrimaries, targetTF, targetPrimaries);

    if (applyTint == 1)
        pixColor = vec4(pixColor.rgb * tint.rgb, pixColor[3]);

    if (radius > 0.0)
        pixColor = rounding(pixColor);
    
    fragColor = pixColor * alpha;
}
