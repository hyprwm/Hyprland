#version 320 es

uniform mat3 proj;

// Per-vertex attributes
in vec2 vertexPos;       // Vertex position in normalized rect space (0-1)
in vec2 vertexTexcoord;  // Vertex UV coordinates

// Per-instance attributes
in vec4 instanceRect;    // x, y, width, height
in vec4 instanceColor;   // r, g, b, a (premultiplied)

// Outputs to fragment shader
out vec4 v_color;
out vec2 v_texcoord;

void main() {
    // Transform vertex position from normalized space to instance space
    vec2 pos = instanceRect.xy + vertexPos * instanceRect.zw;
    
    // Apply projection matrix
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    
    // Pass through color and texcoord
    v_color = instanceColor;
    v_texcoord = vertexTexcoord;
}