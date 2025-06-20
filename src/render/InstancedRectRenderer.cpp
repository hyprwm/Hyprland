#include "InstancedRectRenderer.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"
#include <string>
#include <cstring>

using namespace Hyprutils::Math;

CInstancedRectRenderer::CInstancedRectRenderer() = default;

CInstancedRectRenderer::~CInstancedRectRenderer() {
    // Only delete OpenGL resources if we have a valid context
    // This prevents crashes during compositor shutdown
    if (g_pCompositor && g_pCompositor->m_wlDisplay) {
        if (m_vao)
            glDeleteVertexArrays(1, &m_vao);
        if (m_vboVertex)
            glDeleteBuffers(1, &m_vboVertex);
        if (m_vboInstance)
            glDeleteBuffers(1, &m_vboInstance);
        if (m_ebo)
            glDeleteBuffers(1, &m_ebo);
        if (m_shaderProgram)
            glDeleteProgram(m_shaderProgram);
    }
}

void CInstancedRectRenderer::init(CHyprOpenGLImpl* gl) {
    m_gl = gl;

    // Check if instanced rendering is supported
    m_supported = checkInstancedRenderingSupport();
    if (!m_supported) {
        Debug::log(WARN, "GPU instanced rendering not supported, falling back to batched rendering");
        return;
    }

    // Compile instanced shader
    if (!compileInstancedShader()) {
        m_supported = false;
        Debug::log(ERR, "Failed to compile instanced shader, falling back to batched rendering");
        return;
    }

    setupBuffers();
    Debug::log(LOG, "GPU instanced rendering initialized successfully");
}

bool CInstancedRectRenderer::checkInstancedRenderingSupport() {
    // Check for OpenGL ES 3.0+ or required extensions
    const char* version = (const char*)glGetString(GL_VERSION);
    if (!version)
        return false;

    // Parse version
    int major = 0, minor = 0;
    if (sscanf(version, "OpenGL ES %d.%d", &major, &minor) == 2) {
        if (major >= 3)
            return true;
    }

    // Check for instancing extension on older versions
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (extensions && strstr(extensions, "GL_ARB_instanced_arrays")) {
        return true;
    }

    return false;
}

bool CInstancedRectRenderer::compileInstancedShader() {
    // Read shader sources
    std::string vertexSource, fragmentSource;

    // For now, we'll use hardcoded shader source
    // In production, this should read from the shader files
    vertexSource = R"(#version 320 es
uniform mat3 proj;

// Per-vertex attributes
in vec2 vertexPos;
in vec2 vertexTexcoord;

// Per-instance attributes
in vec4 instanceRect;
in vec4 instanceColor;

out vec4 v_color;
out vec2 v_texcoord;

void main() {
    vec2 pos = instanceRect.xy + vertexPos * instanceRect.zw;
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_color = instanceColor;
    v_texcoord = vertexTexcoord;
})";

    // Use the existing quad fragment shader logic
    fragmentSource = R"(#version 320 es
precision highp float;

uniform float radius;
uniform float roundingPower;
uniform vec4 topLeft;
uniform vec4 fullSize;

in vec4 v_color;
in vec2 v_texcoord;
out vec4 FragColor;

float applyRounding(vec2 pixCoord, vec2 size) {
    if (radius <= 0.0) return 1.0;
    
    vec2 halfSize = size * 0.5;
    vec2 q = abs(pixCoord - halfSize) - halfSize + radius;
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    return smoothstep(0.0, 1.0, -d * roundingPower);
}

void main() {
    vec2 pixCoord = v_texcoord * fullSize.zw;
    float alpha = applyRounding(pixCoord, fullSize.zw);
    FragColor = v_color * alpha;
})";

    // Compile vertex shader
    GLuint      vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vSrc         = vertexSource.c_str();
    glShaderSource(vertexShader, 1, &vSrc, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        Debug::log(ERR, "Instanced vertex shader compilation failed: {}", infoLog);
        glDeleteShader(vertexShader);
        return false;
    }

    // Compile fragment shader
    GLuint      fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fSrc           = fragmentSource.c_str();
    glShaderSource(fragmentShader, 1, &fSrc, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        Debug::log(ERR, "Instanced fragment shader compilation failed: {}", infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    // Link program
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, infoLog);
        Debug::log(ERR, "Instanced shader program linking failed: {}", infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
        return false;
    }

    // Clean up shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Get uniform locations
    m_projUniform          = glGetUniformLocation(m_shaderProgram, "proj");
    m_radiusUniform        = glGetUniformLocation(m_shaderProgram, "radius");
    m_roundingPowerUniform = glGetUniformLocation(m_shaderProgram, "roundingPower");

    return true;
}

void CInstancedRectRenderer::setupBuffers() {
    // Generate VAO, VBOs, EBO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vboVertex);
    glGenBuffers(1, &m_vboInstance);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // Setup vertex data (unit quad)
    const float vertices[] = {
        // pos      texcoord
        0.0f, 0.0f, 0.0f, 0.0f, // Top-left
        1.0f, 0.0f, 1.0f, 0.0f, // Top-right
        1.0f, 1.0f, 1.0f, 1.0f, // Bottom-right
        0.0f, 1.0f, 0.0f, 1.0f  // Bottom-left
    };

    const uint32_t indices[] = {
        0, 1, 2, // First triangle
        0, 2, 3  // Second triangle
    };

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_vboVertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Setup vertex attributes
    GLint posAttrib = glGetAttribLocation(m_shaderProgram, "vertexPos");
    GLint texAttrib = glGetAttribLocation(m_shaderProgram, "vertexTexcoord");

    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Setup instance attributes
    glBindBuffer(GL_ARRAY_BUFFER, m_vboInstance);

    GLint rectAttrib  = glGetAttribLocation(m_shaderProgram, "instanceRect");
    GLint colorAttrib = glGetAttribLocation(m_shaderProgram, "instanceColor");

    glEnableVertexAttribArray(rectAttrib);
    glVertexAttribPointer(rectAttrib, 4, GL_FLOAT, GL_FALSE, FLOATS_PER_INSTANCE * sizeof(float), (void*)0);
    glVertexAttribDivisor(rectAttrib, 1); // Update per instance

    glEnableVertexAttribArray(colorAttrib);
    glVertexAttribPointer(colorAttrib, 4, GL_FLOAT, GL_FALSE, FLOATS_PER_INSTANCE * sizeof(float), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(colorAttrib, 1); // Update per instance

    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void CInstancedRectRenderer::beginBatch(int round, float roundingPower) {
    if (!m_supported)
        return;

    if (m_currentBatch.active) {
        endBatch();
    }

    m_currentBatch.round         = round;
    m_currentBatch.roundingPower = roundingPower;
    m_currentBatch.active        = true;

    m_instanceData.clear();
    m_instanceData.reserve(MAX_INSTANCES_PER_DRAW * FLOATS_PER_INSTANCE);
}

void CInstancedRectRenderer::addRect(const CBox& box, const CHyprColor& color) {
    if (!m_currentBatch.active || !m_supported) {
        return;
    }

    // Check if we've reached the instance limit
    if (m_instanceData.size() >= MAX_INSTANCES_PER_DRAW * FLOATS_PER_INSTANCE) {
        // Flush current batch and start a new one
        endBatch();
        beginBatch(m_currentBatch.round, m_currentBatch.roundingPower);
    }

    // Add instance data: rect (x, y, w, h) and color (r, g, b, a)
    // Premultiply alpha
    const float r = color.r * color.a;
    const float g = color.g * color.a;
    const float b = color.b * color.a;
    const float a = color.a;

    m_instanceData.insert(m_instanceData.end(), {(float)box.x, (float)box.y, (float)box.width, (float)box.height, r, g, b, a});
}

void CInstancedRectRenderer::endBatch() {
    if (!m_currentBatch.active || m_instanceData.empty() || !m_supported) {
        m_currentBatch.active = false;
        return;
    }

    updateInstanceBuffer();
    drawInstances();

    m_currentBatch.active = false;
    m_instanceData.clear();
}

void CInstancedRectRenderer::updateInstanceBuffer() {
    glBindBuffer(GL_ARRAY_BUFFER, m_vboInstance);
    glBufferData(GL_ARRAY_BUFFER, m_instanceData.size() * sizeof(float), m_instanceData.data(), GL_STREAM_DRAW);
}

void CInstancedRectRenderer::drawInstances() {
    if (!m_gl)
        return;

    // Use our instanced shader
    glUseProgram(m_shaderProgram);

    // Set uniforms
    if (m_projUniform >= 0) {
        // Get the current projection matrix
        Mat3x3 glMatrix = m_gl->m_renderData.projection;
        glUniformMatrix3fv(m_projUniform, 1, GL_TRUE, glMatrix.getMatrix().data());
    }

    if (m_radiusUniform >= 0) {
        glUniform1f(m_radiusUniform, (float)m_currentBatch.round);
    }

    if (m_roundingPowerUniform >= 0) {
        glUniform1f(m_roundingPowerUniform, m_currentBatch.roundingPower);
    }

    // Bind VAO and draw
    glBindVertexArray(m_vao);

    const size_t instanceCount = m_instanceData.size() / FLOATS_PER_INSTANCE;
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, instanceCount);

    glBindVertexArray(0);
}