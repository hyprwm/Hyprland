#pragma once

#include "Framebuffer.hpp"

class CMonitor;

class CRenderbuffer {
  public:
    CRenderbuffer(wlr_buffer* buffer, uint32_t format);
    ~CRenderbuffer();

    void          bind();
    void          bindFB();
    void          unbind();
    CFramebuffer* getFB();

    wlr_buffer*   m_pWlrBuffer = nullptr;

    DYNLISTENER(destroyBuffer);

  private:
    EGLImageKHR  m_iImage = 0;
    GLuint       m_iRBO   = 0;
    CFramebuffer m_sFramebuffer;
};