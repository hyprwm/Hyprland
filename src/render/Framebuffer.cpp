#include "Framebuffer.hpp"
#include "OpenGL.hpp"

bool CFramebuffer::alloc(int w, int h, uint32_t drmFormat) {
    bool firstAlloc = false;
    RASSERT((w > 1 && h > 1), "cannot alloc a FB with negative / zero size! (attempted {}x{})", w, h);

    uint32_t glFormat = drmFormatToGL(drmFormat);
    uint32_t glType   = glFormatToType(glFormat);

    if (m_iFb == (uint32_t)-1) {
        firstAlloc = true;
        glGenFramebuffers(1, &m_iFb);
    }

    if (m_cTex.m_iTexID == 0) {
        firstAlloc = true;
        glGenTextures(1, &m_cTex.m_iTexID);
        glBindTexture(GL_TEXTURE_2D, m_cTex.m_iTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    if (firstAlloc || m_vSize != Vector2D(w, h)) {
        glBindTexture(GL_TEXTURE_2D, m_cTex.m_iTexID);
        glTexImage2D(GL_TEXTURE_2D, 0, glFormat, w, h, 0, GL_RGBA, glType, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, m_iFb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_cTex.m_iTexID, 0);

// TODO: Allow this with gles2
#ifndef GLES2
        if (m_pStencilTex) {
            glBindTexture(GL_TEXTURE_2D, m_pStencilTex->m_iTexID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0);

            glBindFramebuffer(GL_FRAMEBUFFER, m_iFb);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_pStencilTex->m_iTexID, 0);
        }
#endif

        auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        RASSERT((status == GL_FRAMEBUFFER_COMPLETE), "Framebuffer incomplete, couldn't create! (FB status: {})", status);

        Debug::log(LOG, "Framebuffer created, status {}", status);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, g_pHyprOpenGL->m_iCurrentOutputFb);

    m_vSize = Vector2D(w, h);

    return true;
}

void CFramebuffer::addStencil() {
    // TODO: Allow this with gles2
#ifndef GLES2
    glBindTexture(GL_TEXTURE_2D, m_pStencilTex->m_iTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_vSize.x, m_vSize.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_iFb);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_pStencilTex->m_iTexID, 0);

    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    RASSERT((status == GL_FRAMEBUFFER_COMPLETE), "Failed adding a stencil to fbo!", status);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, g_pHyprOpenGL->m_iCurrentOutputFb);
#endif
}

void CFramebuffer::bind() {
#ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iFb);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, m_iFb);
#endif
    glViewport(0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.y);
}

void CFramebuffer::release() {
    if (m_iFb != (uint32_t)-1 && m_iFb)
        glDeleteFramebuffers(1, &m_iFb);

    if (m_cTex.m_iTexID)
        glDeleteTextures(1, &m_cTex.m_iTexID);

    m_cTex.m_iTexID = 0;
    m_iFb           = -1;
    m_vSize         = Vector2D();
}

CFramebuffer::~CFramebuffer() {
    release();
}

bool CFramebuffer::isAllocated() {
    return m_iFb != (GLuint)-1;
}