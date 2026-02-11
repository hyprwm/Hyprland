#pragma once

#include "Renderer.hpp"

class CHyprGLRenderer : public IHyprRenderer {
  public:
    CHyprGLRenderer();

    void endRender(const std::function<void()>& renderingDoneCallback = {}) override;

  private:
    bool         beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple = false) override;
    bool         beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, CFramebuffer* fb, bool simple = false) override;
    void         initRender() override;
    bool         initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;
    SP<CTexture> getBackground(PHLMONITOR pMonitor) override;

    void         draw(CBorderPassElement* element, const CRegion& damage) override;
    void         draw(CClearPassElement* element, const CRegion& damage) override;
    void         draw(CFramebufferElement* element, const CRegion& damage) override;
    void         draw(CPreBlurElement* element, const CRegion& damage) override;
    void         draw(CRectPassElement* element, const CRegion& damage) override;
    void         draw(CRendererHintsPassElement* element, const CRegion& damage) override;
    void         draw(CShadowPassElement* element, const CRegion& damage) override;
    void         draw(CSurfacePassElement* element, const CRegion& damage) override;
    void         draw(CTexPassElement* element, const CRegion& damage) override;
    void         draw(CTextureMatteElement* element, const CRegion& damage) override;

    friend class CHyprOpenGLImpl;
};
