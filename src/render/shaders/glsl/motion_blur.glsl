#ifndef MOTION_BLUR_GLSL
#define MOTION_BLUR_GLSL

const int MOTION_BLUR_MAX_SAMPLES = 64;

vec4 motionBlurSample(sampler2D texSampler, vec4 previousBox, vec4 currentBox, vec4 sourceBox, vec2 sourceTexSize, int requestedSamples, bool useRGBA) {
    int   samples    = int(clamp(float(requestedSamples), 1.0, float(MOTION_BLUR_MAX_SAMPLES)));
    vec4 accumulated = vec4(0.0);

    for (int i = 0; i < MOTION_BLUR_MAX_SAMPLES; ++i) {
        if (i >= samples)
            break;

        float t  = float(i) / float(samples);
        vec4  box = mix(currentBox, previousBox, t);
        vec2  uv  = (vec2(gl_FragCoord) - box.xy) / box.zw;

        if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
            continue;

        vec2 sourceUV = (sourceBox.xy + uv * sourceBox.zw) / sourceTexSize;
        vec4 color    = texture(texSampler, sourceUV);
        if (!useRGBA)
            color.a = 1.0;

        accumulated += color;
    }

    if (accumulated.a == 0.0)
        return vec4(0.0);

    return accumulated / float(samples);
}

#endif
