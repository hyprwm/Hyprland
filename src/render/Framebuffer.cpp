#include "Framebuffer.hpp"
#include "OpenGL.hpp"

CFramebuffer::CFramebuffer() {
    ;
}

bool CFramebuffer::alloc(int w, int h, uint32_t drmFormat) {
    bool firstAlloc = false;
    RASSERT((w > 0 && h > 0), "cannot alloc a FB with negative / zero size! (attempted {}x{})", w, h);

    uint32_t glFormat = NFormatUtils::drmFormatToGL(drmFormat);
    uint32_t glType   = NFormatUtils::glFormatToType(glFormat);

    if (!m_cTex) {
        m_cTex = makeShared<CTexture>();
        m_cTex->allocate();
        glBindTexture(GL_TEXTURE_2D, m_cTex->m_iTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        firstAlloc = true;
    }

    if (!m_iFbAllocated) {
        glGenFramebuffers(1, &m_iFb);
        m_iFbAllocated = true;
        firstAlloc     = true;
    }

    if (firstAlloc || m_vSize != Vector2D(w, h)) {
        glBindTexture(GL_TEXTURE_2D, m_cTex->m_iTexID);
        glTexImage2D(GL_TEXTURE_2D, 0, glFormat, w, h, 0, GL_RGBA, glType, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, m_iFb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_cTex->m_iTexID, 0);

// TODO: Allow this with gles2
#ifndef GLES2
        if (m_pStencilTex) {
            glBindTexture(GL_TEXTURE_2D, m_pStencilTex->m_iTexID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_pStencilTex->m_iTexID, 0);
        }
#endif

        auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        RASSERT((status == GL_FRAMEBUFFER_COMPLETE), "Framebuffer incomplete, couldn't create! (FB status: {}, GL Error: 0x{:x})", status, (int)glGetError());

        Debug::log(LOG, "Framebuffer created, status {}", status);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_vSize = Vector2D(w, h);

    return true;
}

void CFramebuffer::addStencil(SP<CTexture> tex) {
    // TODO: Allow this with gles2
#ifndef GLES2
    m_pStencilTex = tex;
    glBindTexture(GL_TEXTURE_2D, m_pStencilTex->m_iTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_vSize.x, m_vSize.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, m_iFb);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_pStencilTex->m_iTexID, 0);

    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    RASSERT((status == GL_FRAMEBUFFER_COMPLETE), "Failed adding a stencil to fbo!", status);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
}

void CFramebuffer::bind() {
#ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iFb);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, m_iFb);
#endif

    if (g_pHyprOpenGL)
        glViewport(0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.y);
    else
        glViewport(0, 0, m_vSize.x, m_vSize.y);
}

void CFramebuffer::unbind() {
#ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
}

void CFramebuffer::release() {
    if (m_iFbAllocated) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_iFb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glDeleteFramebuffers(1, &m_iFb);
        m_iFbAllocated = false;
        m_iFb          = 0;
    }

    if (m_cTex)
        m_cTex.reset();

    m_vSize = Vector2D();
}

CFramebuffer::~CFramebuffer() {
    release();
}

bool CFramebuffer::isAllocated() {
    return m_iFbAllocated && m_cTex;
}

SP<CTexture> CFramebuffer::getTexture() {
    return m_cTex;
}

GLuint CFramebuffer::getFBID() {
    return m_iFbAllocated ? m_iFb : 0;
}

SP<CTexture> CFramebuffer::getStencilTex() {
    return m_pStencilTex;
}
