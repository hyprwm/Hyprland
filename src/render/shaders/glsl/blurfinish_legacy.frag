precision         highp float;
varying vec2      v_texcoord; // is in 0-1
uniform sampler2D tex;

uniform float     noise;
uniform float     brightness;

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 1689.1984);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec4 pixColor = texture2D(tex, v_texcoord);

    // noise
    float noiseHash   = hash(v_texcoord);
    float noiseAmount = (mod(noiseHash, 1.0) - 0.5);
    pixColor.rgb += noiseAmount * noise;

    // brightness
    if (brightness < 1.0) {
        pixColor.rgb *= brightness;
    }

    gl_FragColor = pixColor;
}
