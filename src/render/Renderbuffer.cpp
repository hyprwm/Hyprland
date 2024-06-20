#include "Renderbuffer.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"
#include "../protocols/types/Buffer.hpp"

#include <dlfcn.h>

CRenderbuffer::~CRenderbuffer() {
    if (!g_pCompositor || g_pCompositor->m_bIsShuttingDown || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->makeEGLCurrent();

    unbind();
    m_sFramebuffer.release();
    glDeleteRenderbuffers(1, &m_iRBO);

    g_pHyprOpenGL->m_sProc.eglDestroyImageKHR(g_pHyprOpenGL->m_pEglDisplay, m_iImage);
}

CRenderbuffer::CRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t format) : m_pHLBuffer(buffer), m_uDrmFormat(format) {
    auto dma = buffer->dmabuf();

    m_iImage = g_pHyprOpenGL->createEGLImage(dma);
    if (m_iImage == EGL_NO_IMAGE_KHR)
        throw std::runtime_error("createEGLImage failed");

    glGenRenderbuffers(1, &m_iRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_iRBO);
    g_pHyprOpenGL->m_sProc.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)m_iImage);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_sFramebuffer.m_iFb);
    m_sFramebuffer.m_vSize = buffer->size;
    m_sFramebuffer.bind();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_iRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("rbo: glCheckFramebufferStatus failed");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    listeners.destroyBuffer = buffer->events.destroy.registerListener([this](std::any d) { g_pHyprRenderer->onRenderbufferDestroy(this); });
}

void CRenderbuffer::bind() {
    glBindRenderbuffer(GL_RENDERBUFFER, m_iRBO);
    bindFB();
}

void CRenderbuffer::bindFB() {
    m_sFramebuffer.bind();
}

void CRenderbuffer::unbind() {
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
#ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
}

CFramebuffer* CRenderbuffer::getFB() {
    return &m_sFramebuffer;
}