#pragma once

#include "../defines.hpp"
#include "Texture.hpp"

class CFramebuffer {
  public:
    ~CFramebuffer();

    bool                alloc(int w, int h);
    void                bind();
    void                release();
    void                reset();
    bool                isAllocated();

    Vector2D            m_Position;
    Vector2D            m_Size;
    float               m_fScale = 1;

    CTexture            m_cTex;
    GLuint              m_iFb = -1;

    CTexture*           m_pStencilTex = nullptr;

    wl_output_transform m_tTransform; // for saving state
};