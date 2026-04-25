vec3 gain(vec3 src, float k) {
    vec3 x = clamp(src, 0.0, 1.0);
    vec3 t = step(0.5, x);
    vec3 y = mix(x, 1.0 - x, t);
    vec3 a = 0.5 * pow(2.0 * y, vec3(k));
    return mix(a, 1.0 - a, t);
}
