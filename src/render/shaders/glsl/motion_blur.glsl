#ifndef MOTION_BLUR_GLSL
#define MOTION_BLUR_GLSL

const int MOTION_BLUR_MAX_SAMPLES = 64;

int motionBlurSampleCount(int requestedSamples) {
    return int(clamp(float(requestedSamples), 1.0, float(MOTION_BLUR_MAX_SAMPLES)));
}

// false if this fragment is outside of the box at sample i
bool motionBlurSourceUV(int i, int samples, vec4 previousBox, vec4 currentBox, vec4 sourceBox, vec2 sourceTexOrigin, vec2 sourceTexSize, out vec2 sourceUV) {
    float t   = float(i) / float(samples);
    vec4  box = mix(currentBox, previousBox, t);
    if (box.z <= 0.0 || box.w <= 0.0)
        return false;

    vec2 uv = (vec2(gl_FragCoord) - box.xy) / box.zw;

    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
        return false;

    sourceUV = (sourceBox.xy + uv * sourceBox.zw - sourceTexOrigin) / sourceTexSize;
    return true;
}

vec4 motionBlurResolve(vec4 accumulated, int samples) {
    if (accumulated.a == 0.0)
        return vec4(0.0);

    return accumulated / float(samples);
}

vec4 motionBlurSample(sampler2D texSampler, vec4 previousBox, vec4 currentBox, vec4 sourceBox, vec2 sourceTexOrigin, vec2 sourceTexSize, int requestedSamples,
                      bool useRGBA) {
    int  samples     = motionBlurSampleCount(requestedSamples);
    vec4 accumulated = vec4(0.0);

    for (int i = 0; i < MOTION_BLUR_MAX_SAMPLES; ++i) {
        if (i >= samples)
            break;

        vec2 sourceUV;
        if (!motionBlurSourceUV(i, samples, previousBox, currentBox, sourceBox, sourceTexOrigin, sourceTexSize, sourceUV))
            continue;

        vec4 color = texture(texSampler, sourceUV);
        if (!useRGBA)
            color.a = 1.0;

        accumulated += color;
    }

    return motionBlurResolve(accumulated, samples);
}

#if USE_EXTERNAL
vec4 motionBlurSample(samplerExternalOES texSampler, vec4 previousBox, vec4 currentBox, vec4 sourceBox, vec2 sourceTexOrigin, vec2 sourceTexSize, int requestedSamples,
                      bool useRGBA) {
    int  samples     = motionBlurSampleCount(requestedSamples);
    vec4 accumulated = vec4(0.0);

    for (int i = 0; i < MOTION_BLUR_MAX_SAMPLES; ++i) {
        if (i >= samples)
            break;

        vec2 sourceUV;
        if (!motionBlurSourceUV(i, samples, previousBox, currentBox, sourceBox, sourceTexOrigin, sourceTexSize, sourceUV))
            continue;

        vec4 color = texture(texSampler, sourceUV);
        if (!useRGBA)
            color.a = 1.0;

        accumulated += color;
    }

    return motionBlurResolve(accumulated, samples);
}
#endif

#endif
