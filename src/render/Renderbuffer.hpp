#pragma once

#include "../helpers/signal/Signal.hpp"
#include "../helpers/memory/Memory.hpp"
#include "Framebuffer.hpp"
#include <aquamarine/buffer/Buffer.hpp>

class CMonitor;

class CRenderbuffer {
  public:
    CRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format);
    ~CRenderbuffer();

    bool                    good();
    void                    bind();
    void                    bindFB();
    void                    unbind();
    CFramebuffer*           getFB();
    uint32_t                getFormat();

    WP<Aquamarine::IBuffer> m_pHLBuffer;

  private:
    void*        m_iImage = nullptr;
    GLuint       m_iRBO   = 0;
    CFramebuffer m_sFramebuffer;
    uint32_t     m_uDrmFormat = 0;
    bool         m_bGood      = false;

    struct {
        CHyprSignalListener destroyBuffer;
        m_m_listeners;
    };