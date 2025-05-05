#include "FramebufferElement.hpp"
#include "../OpenGL.hpp"

CFramebufferElement::CFramebufferElement(const CFramebufferElement::SFramebufferElementData& data_) : m_data(data_) {
    ;
}

void CFramebufferElement::draw(const CRegion& damage) {
    CFramebuffer* fb = nullptr;

    if (m_data.main) {
        switch (m_data.framebufferID) {
            case FB_MONITOR_RENDER_MAIN: fb = g_pHyprOpenGL->m_renderData.mainFB; break;
            case FB_MONITOR_RENDER_CURRENT: fb = g_pHyprOpenGL->m_renderData.currentFB; break;
            case FB_MONITOR_RENDER_OUT: fb = g_pHyprOpenGL->m_renderData.outFB; break;
        }

        if (!fb) {
            Debug::log(ERR, "BUG THIS: CFramebufferElement::draw: main but null");
            return;
        }

    } else {
        switch (m_data.framebufferID) {
            case FB_MONITOR_RENDER_EXTRA_OFFLOAD: fb = &g_pHyprOpenGL->m_renderData.pCurrentMonData->offloadFB; break;
            case FB_MONITOR_RENDER_EXTRA_MIRROR: fb = &g_pHyprOpenGL->m_renderData.pCurrentMonData->mirrorFB; break;
            case FB_MONITOR_RENDER_EXTRA_MIRROR_SWAP: fb = &g_pHyprOpenGL->m_renderData.pCurrentMonData->mirrorSwapFB; break;
            case FB_MONITOR_RENDER_EXTRA_OFF_MAIN: fb = &g_pHyprOpenGL->m_renderData.pCurrentMonData->offMainFB; break;
            case FB_MONITOR_RENDER_EXTRA_MONITOR_MIRROR: fb = &g_pHyprOpenGL->m_renderData.pCurrentMonData->monitorMirrorFB; break;
            case FB_MONITOR_RENDER_EXTRA_BLUR: fb = &g_pHyprOpenGL->m_renderData.pCurrentMonData->blurFB; break;
        }

        if (!fb) {
            Debug::log(ERR, "BUG THIS: CFramebufferElement::draw: not main but null");
            return;
        }
    }

    fb->bind();
}

bool CFramebufferElement::needsLiveBlur() {
    return false;
}

bool CFramebufferElement::needsPrecomputeBlur() {
    return false;
}

bool CFramebufferElement::undiscardable() {
    return true;
}
