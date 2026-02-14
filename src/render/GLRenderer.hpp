#pragma once

#include "Renderer.hpp"

class CHyprGLRenderer : public IHyprRenderer {
  public:
    CHyprGLRenderer();

    void         endRender(const std::function<void()>& renderingDoneCallback = {}) override;
    SP<ITexture> createTexture(bool opaque = false) override;
    SP<ITexture> createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false) override;
    SP<ITexture> createTexture(const Aquamarine::SDMABUFAttrs&, void* image, bool opaque = false) override;
    SP<ITexture> createTexture(const int width, const int height, unsigned char* const data) override;
    SP<ITexture> createTexture(cairo_surface_t* cairo) override;
    void*        createImage(const SP<Aquamarine::IBuffer> buffer) override;

  private:
    bool         beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple = false) override;
    bool         beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, CFramebuffer* fb, bool simple = false) override;
    void         initRender() override;
    bool         initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

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

    SP<ITexture> getBlurTexture(PHLMONITORREF pMonitor) override;

    friend class CHyprOpenGLImpl;
};
