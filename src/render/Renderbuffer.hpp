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

    WP<Aquamarine::IBuffer> m_hlBuffer;

  private:
    void*        m_image = nullptr;
    GLuint       m_rbo   = 0;
    CFramebuffer m_framebuffer;
    uint32_t     m_drmFormat = 0;
    bool         m_good      = false;

    struct {
        CHyprSignalListener destroyBuffer;
    } m_listeners;
};