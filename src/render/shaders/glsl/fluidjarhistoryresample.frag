#version 300 es

precision highp float;

in vec2 v_texcoord;
uniform sampler2D fluidJarHistoryTex;
uniform vec2 fluidJarOldResolution;
uniform vec4 fluidJarHistoryTransform;
uniform vec4 fluidJarHistoryFallback;

layout(location = 0) out vec4 fragColor;

void main() {
    vec2 oldPosition = gl_FragCoord.xy * fluidJarHistoryTransform.xy + fluidJarHistoryTransform.zw;
    if (any(lessThan(oldPosition, vec2(0.0))) || any(greaterThanEqual(oldPosition, fluidJarOldResolution))) {
        fragColor = fluidJarHistoryFallback;
        return;
    }

    fragColor = texture(fluidJarHistoryTex, oldPosition / fluidJarOldResolution);
}
