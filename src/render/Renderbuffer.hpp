#pragma once

#include "Framebuffer.hpp"
#include <aquamarine/buffer/Buffer.hpp>

class CMonitor;

class CRenderbuffer {
  public:
    CRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format);
    ~CRenderbuffer();

    void                    bind();
    void                    bindFB();
    void                    unbind();
    CFramebuffer*           getFB();
    uint32_t                getFormat();

    WP<Aquamarine::IBuffer> m_pHLBuffer;

  private:
    EGLImageKHR  m_iImage = 0;
    GLuint       m_iRBO   = 0;
    CFramebuffer m_sFramebuffer;
    uint32_t     m_uDrmFormat = 0;

    struct {
        CHyprSignalListener destroyBuffer;
    } listeners;
};