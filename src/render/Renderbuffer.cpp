#include "Renderbuffer.hpp"
#include "Renderer.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"
#include "../protocols/types/Buffer.hpp"
#include <hyprutils/signal/Listener.hpp>
#include <hyprutils/signal/Signal.hpp>

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
    if (m_iImage == EGL_NO_IMAGE_KHR) {
        NDebug::log(ERR, "rb: createEGLImage failed");
        return;
    }

    glGenRenderbuffers(1, &m_iRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_iRBO);
    g_pHyprOpenGL->m_sProc.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)m_iImage);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_sFramebuffer.m_iFb);
    m_sFramebuffer.m_iFbAllocated = true;
    m_sFramebuffer.m_vSize        = buffer->size;
    m_sFramebuffer.bind();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_iRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        NDebug::log(ERR, "rbo: glCheckFramebufferStatus failed");
        return;
    }

    m_sFramebuffer.unbind();

    listeners.destroyBuffer = buffer->events.destroy.registerListener([this](std::any d) { g_pHyprRenderer->onRenderbufferDestroy(this); });

    m_bGood = true;
}

bool CRenderbuffer::good() {
    return m_bGood;
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
    m_sFramebuffer.unbind();
}

CFramebuffer* CRenderbuffer::getFB() {
    return &m_sFramebuffer;
}
