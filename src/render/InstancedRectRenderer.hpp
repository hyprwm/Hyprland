#pragma once

#include <vector>
#include <GLES3/gl32.h>
#include "../helpers/Color.hpp"
#include "../helpers/math/Math.hpp"
#include "Shader.hpp"

class CHyprOpenGLImpl;

// GPU instanced renderer for drawing many rectangles with the same shader state
class CInstancedRectRenderer {
  public:
    CInstancedRectRenderer();
    ~CInstancedRectRenderer();

    void   init(CHyprOpenGLImpl* gl);
    void   beginBatch(int round, float roundingPower);
    void   addRect(const CBox& box, const CHyprColor& color);
    void   endBatch();

    size_t getBatchSize() const {
        return m_instanceData.size() / FLOATS_PER_INSTANCE;
    }

    // Performance metrics
    size_t getMaxInstancesPerDraw() const {
        return MAX_INSTANCES_PER_DRAW;
    }
    bool isInstancedRenderingSupported() const {
        return m_supported;
    }

  private:
    static constexpr size_t MAX_INSTANCES_PER_DRAW = 10000;
    static constexpr size_t FLOATS_PER_INSTANCE    = 8; // x, y, w, h, r, g, b, a

    struct SBatchState {
        int   round;
        float roundingPower;
        bool  active = false;
    };

    CHyprOpenGLImpl* m_gl = nullptr;
    SBatchState      m_currentBatch;
    bool             m_supported = false;

    // Instance data
    std::vector<float> m_instanceData;

    // OpenGL objects
    GLuint m_vao         = 0;
    GLuint m_vboVertex   = 0; // Vertex data (shared for all instances)
    GLuint m_vboInstance = 0; // Instance data (per-rect attributes)
    GLuint m_ebo         = 0; // Element buffer (shared for all instances)

    // Shader for instanced rendering
    GLuint m_shaderProgram        = 0;
    GLint  m_projUniform          = -1;
    GLint  m_radiusUniform        = -1;
    GLint  m_roundingPowerUniform = -1;

    void   setupBuffers();
    void   updateInstanceBuffer();
    void   drawInstances();
    bool   checkInstancedRenderingSupport();
    bool   compileInstancedShader();
};