#pragma once

#include "Renderer.hpp"

class CHyprGLRenderer : public IHyprRenderer {
  public:
    CHyprGLRenderer();

    void endRender(const std::function<void()>& renderingDoneCallback = {}) override;

  private:
    bool beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IHLBuffer> buffer = {}, CFramebuffer* fb = nullptr, bool simple = false) override;
    bool beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, CFramebuffer* fb, bool simple = false) override;
    void initRender() override;
    bool initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

    friend class CHyprOpenGLImpl;
};
