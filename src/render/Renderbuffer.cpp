#include "Renderbuffer.hpp"
#include "Renderer.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"
#include "../protocols/types/Buffer.hpp"
#include <hyprutils/signal/Listener.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <dlfcn.h>

CRenderbuffer::~CRenderbuffer() {
    if (!g_pCompositor || g_pCompositor->m_isShuttingDown || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->makeEGLCurrent();

    unbind();
    m_framebuffer.release();
    glDeleteRenderbuffers(1, &m_rbo);

    g_pHyprOpenGL->m_proc.eglDestroyImageKHR(g_pHyprOpenGL->m_eglDisplay, m_image);
}

CRenderbuffer::CRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format) : m_hlBuffer(buffer), m_drmFormat(format) {
    auto dma = buffer->dmabuf();

    m_image = g_pHyprOpenGL->createEGLImage(dma);
    if (m_image == EGL_NO_IMAGE_KHR) {
        Debug::log(ERR, "rb: createEGLImage failed");
        return;
    }

    glGenRenderbuffers(1, &m_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    g_pHyprOpenGL->m_proc.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)m_image);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_framebuffer.m_fb);
    m_framebuffer.m_fbAllocated = true;
    m_framebuffer.m_size        = buffer->size;
    m_framebuffer.bind();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Debug::log(ERR, "rbo: glCheckFramebufferStatus failed");
        return;
    }

    m_framebuffer.unbind();

    m_listeners.destroyBuffer = buffer->events.destroy.registerListener([this](std::any d) { g_pHyprRenderer->onRenderbufferDestroy(this); });

    m_good = true;
}

bool CRenderbuffer::good() {
    return m_good;
}

void CRenderbuffer::bind() {
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    bindFB();
}

void CRenderbuffer::bindFB() {
    m_framebuffer.bind();
}

void CRenderbuffer::unbind() {
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    m_framebuffer.unbind();
}

CFramebuffer* CRenderbuffer::getFB() {
    return &m_framebuffer;
}
