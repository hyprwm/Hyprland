#pragma once

#include <vector>
#include <GLES3/gl32.h>
#include "../helpers/Color.hpp"
#include "../helpers/math/Math.hpp"
#include "Shader.hpp"

class CHyprOpenGLImpl;

// Optimized renderer for batching multiple rectangles with the same shader state
class CBatchedRectRenderer {
  public:
    CBatchedRectRenderer();
    ~CBatchedRectRenderer();

    void init(CHyprOpenGLImpl* gl);
    void beginBatch(int round, float roundingPower);
    void addRect(const CBox& box, const CHyprColor& color);
    void endBatch();
    
    size_t getBatchSize() const { return m_vertices.size() / VERTICES_PER_RECT; }

  private:
    static constexpr size_t VERTICES_PER_RECT = 4;
    static constexpr size_t FLOATS_PER_VERTEX = 8; // x, y, r, g, b, a, u, v
    
    struct SBatchState {
        int   round;
        float roundingPower;
        bool  active = false;
    };
    
    CHyprOpenGLImpl*  m_gl = nullptr;
    SBatchState       m_currentBatch;
    
    // Vertex data
    std::vector<float> m_vertices;
    std::vector<uint32_t> m_indices;
    
    // OpenGL objects
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    
    void setupBuffers();
    void updateBuffers();
    void drawBatch();
};