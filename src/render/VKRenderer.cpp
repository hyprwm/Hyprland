#include "VKRenderer.hpp"

CHyprVKRenderer::CHyprVKRenderer() : IHyprRenderer() {}

bool CHyprVKRenderer::beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IHLBuffer> buffer, CFramebuffer* fb, bool simple) {
    g_pHyprVulkan->begin();
    return true;
}

void CHyprVKRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    g_pHyprVulkan->end();
    if (renderingDoneCallback)
        renderingDoneCallback();
}
