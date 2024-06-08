#pragma once

#include "Framebuffer.hpp"

class CMonitor;
class IWLBuffer;

class CRenderbuffer {
  public:
    CRenderbuffer(wlr_buffer* buffer, uint32_t format);
    CRenderbuffer(SP<IWLBuffer> buffer, uint32_t format);
    ~CRenderbuffer();

    void          bind();
    void          bindFB();
    void          unbind();
    CFramebuffer* getFB();
    uint32_t      getFormat();

    wlr_buffer*   m_pWlrBuffer = nullptr;
    WP<IWLBuffer> m_pHLBuffer  = {};

    DYNLISTENER(destroyBuffer);

  private:
    EGLImageKHR  m_iImage = 0;
    GLuint       m_iRBO   = 0;
    CFramebuffer m_sFramebuffer;
    uint32_t     m_uDrmFormat = 0;
};