float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 1689.1984);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec4 blurFinish(vec4 pixColor, vec2 v_texcoord, float noise, float brightness) {
    // noise
    float noiseHash   = hash(v_texcoord);
    float noiseAmount = noiseHash - 0.5;
    pixColor.rgb += noiseAmount * noise;

    // brightness
    pixColor.rgb *= min(1.0, brightness);

    return pixColor;
}
