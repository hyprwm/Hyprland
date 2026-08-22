#ifndef ALLOW_INCLUDES
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable
#endif

#include "blurFinish.glsl"

vec4 glassFinish(vec2 normal) {
    normal /= max(1.0, length(normal));

    vec2 uvStep = normal.x * dFdx(v_texcoord) + normal.y * dFdy(v_texcoord);
    vec2 displacedUV = clamp(v_texcoord + glassRefraction * uvStep, vec2(0.0), vec2(1.0));
    vec4 pixColor = texture(tex, displacedUV);

    const vec2 LIGHT_DIRECTION = vec2(-0.451219, 0.892413);
    float emboss = dot(normal, LIGHT_DIRECTION);
    pixColor.rgb *= 1.0 + emboss * glassRoughness * 0.12;

    return blurFinish(pixColor, v_texcoord, noise, brightness
#if USE_CM
                      ,
                      sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
