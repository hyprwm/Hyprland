#include "BatchedRectRenderer.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"

CBatchedRectRenderer::CBatchedRectRenderer() = default;

CBatchedRectRenderer::~CBatchedRectRenderer() {
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_ebo)
        glDeleteBuffers(1, &m_ebo);
}

void CBatchedRectRenderer::init(CHyprOpenGLImpl* gl) {
    m_gl = gl;
    setupBuffers();
}

void CBatchedRectRenderer::setupBuffers() {
    // Generate VAO, VBO, EBO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Setup vertex attributes
    // Position (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color (r, g, b, a)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // UV coords (u, v)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

    glBindVertexArray(0);
}

void CBatchedRectRenderer::beginBatch(int round, float roundingPower) {
    if (m_currentBatch.active) {
        endBatch();
    }

    m_currentBatch.round         = round;
    m_currentBatch.roundingPower = roundingPower;
    m_currentBatch.active        = true;

    m_vertices.clear();
    m_indices.clear();
}

void CBatchedRectRenderer::addRect(const CBox& box, const CHyprColor& color) {
    if (!m_currentBatch.active || !m_gl) {
        return;
    }

    // Calculate vertex positions
    const float x1 = box.x;
    const float y1 = box.y;
    const float x2 = box.x + box.width;
    const float y2 = box.y + box.height;

    // Premultiply alpha
    const float r = color.r * color.a;
    const float g = color.g * color.a;
    const float b = color.b * color.a;
    const float a = color.a;

    // Current rect index
    const uint32_t baseIndex = m_vertices.size() / FLOATS_PER_VERTEX;

    // Add vertices (counter-clockwise)
    // Top-left
    m_vertices.insert(m_vertices.end(), {x1, y1, r, g, b, a, 0.0f, 0.0f});
    // Top-right
    m_vertices.insert(m_vertices.end(), {x2, y1, r, g, b, a, 1.0f, 0.0f});
    // Bottom-right
    m_vertices.insert(m_vertices.end(), {x2, y2, r, g, b, a, 1.0f, 1.0f});
    // Bottom-left
    m_vertices.insert(m_vertices.end(), {x1, y2, r, g, b, a, 0.0f, 1.0f});

    // Add indices for two triangles
    m_indices.insert(m_indices.end(),
                     {
                         baseIndex + 0, baseIndex + 1, baseIndex + 2, // First triangle
                         baseIndex + 0, baseIndex + 2, baseIndex + 3  // Second triangle
                     });
}

void CBatchedRectRenderer::endBatch() {
    if (!m_currentBatch.active || m_vertices.empty() || !m_gl) {
        m_currentBatch.active = false;
        return;
    }

    updateBuffers();
    drawBatch();

    m_currentBatch.active = false;
    m_vertices.clear();
    m_indices.clear();
}

void CBatchedRectRenderer::updateBuffers() {
    glBindVertexArray(m_vao);

    // Update vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(float), m_vertices.data(), GL_STREAM_DRAW);

    // Update index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint32_t), m_indices.data(), GL_STREAM_DRAW);

    glBindVertexArray(0);
}

void CBatchedRectRenderer::drawBatch() {
    if (!m_gl || !m_gl->m_shaders) {
        return;
    }

    // Use the QUAD shader
    auto& shader = m_gl->m_shaders->m_shQUAD;
    m_gl->useProgram(shader.program);

    // Set uniforms that are constant for the batch
    glUniform1f(shader.radius, m_currentBatch.round);
    glUniform1f(shader.roundingPower, m_currentBatch.roundingPower);

    // Bind VAO and draw
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}