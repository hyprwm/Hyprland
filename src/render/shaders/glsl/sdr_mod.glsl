uniform float sdrSaturation;
uniform float sdrBrightnessMultiplier;

vec4 saturate(vec4 color, mat3 primaries, float saturation) {
    if (saturation == 1.0)
        return color;
    vec3 brightness = vec3(primaries[1][0], primaries[1][1], primaries[1][2]);
    float Y = dot(color.rgb, brightness);
    return vec4(mix(vec3(Y), color.rgb, saturation), color[3]);
}
