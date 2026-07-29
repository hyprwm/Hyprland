#version 300 es

precision highp float;
precision highp usampler2D;

in vec2 v_texcoord;
uniform usampler2D fluidJarHistoryTex;
uniform vec2 fluidJarOldResolution;
uniform vec4 fluidJarHistoryTransform;

layout(location = 0) out uvec4 fragColor;

void main() {
    vec2 oldPosition = gl_FragCoord.xy * fluidJarHistoryTransform.xy + fluidJarHistoryTransform.zw;
    if (any(lessThan(oldPosition, vec2(0.0))) || any(greaterThanEqual(oldPosition, fluidJarOldResolution))) {
        fragColor = uvec4(0u);
        return;
    }

    fragColor = texelFetch(fluidJarHistoryTex, ivec2(oldPosition), 0);
}
