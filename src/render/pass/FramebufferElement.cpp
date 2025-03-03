#include "FramebufferElement.hpp"
#include "../OpenGL.hpp"

CFramebufferElement::CFramebufferElement(const CFramebufferElement::SFramebufferElementData& data_) : data(data_) {
    ;
}

void CFramebufferElement::draw(const CRegion& damage) {
    CFramebuffer* fb = nullptr;

    if (data.main) {
        switch (data.framebufferID) {
            case FB_MONITOR_RENDER_MAIN: fb = g_pHyprOpenGL->m_RenderData.mainFB; break;
            case FB_MONITOR_RENDER_CURRENT: fb = g_pHyprOpenGL->m_RenderData.currentFB; break;
            case FB_MONITOR_RENDER_OUT: fb = g_pHyprOpenGL->m_RenderData.outFB; break;
        }

        if (!fb) {
            NDebug::log(ERR, "BUG THIS: CFramebufferElement::draw: main but null");
            return;
        }

    } else {
        switch (data.framebufferID) {
            case FB_MONITOR_RENDER_EXTRA_OFFLOAD: fb = &g_pHyprOpenGL->m_RenderData.pCurrentMonData->offloadFB; break;
            case FB_MONITOR_RENDER_EXTRA_MIRROR: fb = &g_pHyprOpenGL->m_RenderData.pCurrentMonData->mirrorFB; break;
            case FB_MONITOR_RENDER_EXTRA_MIRROR_SWAP: fb = &g_pHyprOpenGL->m_RenderData.pCurrentMonData->mirrorSwapFB; break;
            case FB_MONITOR_RENDER_EXTRA_OFF_MAIN: fb = &g_pHyprOpenGL->m_RenderData.pCurrentMonData->offMainFB; break;
            case FB_MONITOR_RENDER_EXTRA_MONITOR_MIRROR: fb = &g_pHyprOpenGL->m_RenderData.pCurrentMonData->monitorMirrorFB; break;
            case FB_MONITOR_RENDER_EXTRA_BLUR: fb = &g_pHyprOpenGL->m_RenderData.pCurrentMonData->blurFB; break;
        }

        if (!fb) {
            NDebug::log(ERR, "BUG THIS: CFramebufferElement::draw: not main but null");
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
