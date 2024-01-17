#include "Renderbuffer.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"

#include <dlfcn.h>

CRenderbuffer::~CRenderbuffer() {
    if (!g_pCompositor)
        return;

    if (eglGetCurrentContext() != wlr_egl_get_context(g_pCompositor->m_sWLREGL))
        eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(g_pCompositor->m_sWLREGL));

    m_sFramebuffer.release();
    glDeleteRenderbuffers(1, &m_iRBO);

    g_pHyprOpenGL->m_sProc.eglDestroyImageKHR(wlr_egl_get_display(g_pCompositor->m_sWLREGL), m_iImage);
}

CRenderbuffer::CRenderbuffer(wlr_buffer* buffer, uint32_t format) : m_pWlrBuffer(buffer) {

    // EVIL, but we can't include a hidden header because nixos is fucking special
    static EGLImageKHR (*PWLREGLCREATEIMAGEFROMDMABUF)(wlr_egl*, wlr_dmabuf_attributes*, bool*);
    static bool symbolFound = false;
    if (!symbolFound) {
        PWLREGLCREATEIMAGEFROMDMABUF = reinterpret_cast<EGLImageKHR (*)(wlr_egl*, wlr_dmabuf_attributes*, bool*)>(dlsym(RTLD_DEFAULT, "wlr_egl_create_image_from_dmabuf"));

        symbolFound = true;

        RASSERT(PWLREGLCREATEIMAGEFROMDMABUF, "wlr_egl_create_image_from_dmabuf was not found in wlroots!");

        Debug::log(LOG, "CRenderbuffer: wlr_egl_create_image_from_dmabuf found at {:x}", (uintptr_t)PWLREGLCREATEIMAGEFROMDMABUF);
    }
    // end evil hack

    struct wlr_dmabuf_attributes dmabuf = {0};
    if (!wlr_buffer_get_dmabuf(buffer, &dmabuf))
        throw std::runtime_error("wlr_buffer_get_dmabuf failed");

    bool externalOnly;
    m_iImage = PWLREGLCREATEIMAGEFROMDMABUF(g_pCompositor->m_sWLREGL, &dmabuf, &externalOnly);
    if (m_iImage == EGL_NO_IMAGE_KHR)
        throw std::runtime_error("wlr_egl_create_image_from_dmabuf failed");

    glGenRenderbuffers(1, &m_iRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_iRBO);
    g_pHyprOpenGL->m_sProc.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)m_iImage);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_sFramebuffer.m_iFb);
    m_sFramebuffer.m_vSize = {buffer->width, buffer->height};
    m_sFramebuffer.bind();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_iRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("rbo: glCheckFramebufferStatus failed");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    hyprListener_destroyBuffer.initCallback(
        &buffer->events.destroy, [](void* owner, void* data) { g_pHyprRenderer->onRenderbufferDestroy((CRenderbuffer*)owner); }, this, "CRenderbuffer");
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