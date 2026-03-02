#pragma once

#include "Renderer.hpp"

class CHyprGLRenderer : public IHyprRenderer {
  public:
    CHyprGLRenderer();

    void                    endRender(const std::function<void()>& renderingDoneCallback = {}) override;
    SP<ITexture>            createStencilTexture(const int width, const int height) override;
    SP<ITexture>            createTexture(bool opaque = false) override;
    SP<ITexture>            createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false) override;
    SP<ITexture>            createTexture(const Aquamarine::SDMABUFAttrs&, bool opaque = false) override;
    SP<ITexture>            createTexture(const int width, const int height, unsigned char* const data) override;
    SP<ITexture>            createTexture(cairo_surface_t* cairo) override;
    SP<ITexture>            createTexture(std::span<const float> lut3D, size_t N) override;
    bool                    explicitSyncSupported() override;
    std::vector<SDRMFormat> getDRMFormats() override;
    std::vector<uint64_t>   getDRMFormatModifiers(DRMFormat format) override;
    SP<IFramebuffer>        createFB(const std::string& name = "") override;
    void                    disableScissor() override;
    void                    blend(bool enabled) override;
    void                    drawShadow(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) override;
    SP<ITexture>            blurFramebuffer(SP<IFramebuffer> source, float a, CRegion* originalDamage) override;
    void                    setViewport(int x, int y, int width, int height) override;
    bool                    reloadShaders(const std::string& path = "") override;

    void                    unsetEGL();

  private:
    void              renderOffToMain(IFramebuffer* off) override;
    SP<IRenderbuffer> getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;
    bool              beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple = false) override;
    bool              beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple = false) override;
    void              initRender() override;
    bool              initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

    void              draw(CBorderPassElement* element, const CRegion& damage) override;
    void              draw(CClearPassElement* element, const CRegion& damage) override;
    void              draw(CFramebufferElement* element, const CRegion& damage) override;
    void              draw(CPreBlurElement* element, const CRegion& damage) override;
    void              draw(CRectPassElement* element, const CRegion& damage) override;
    void              draw(CShadowPassElement* element, const CRegion& damage) override;
    void              draw(CTexPassElement* element, const CRegion& damage) override;
    void              draw(CTextureMatteElement* element, const CRegion& damage) override;

    SP<ITexture>      getBlurTexture(PHLMONITORREF pMonitor) override;

    SP<IRenderbuffer> m_currentRenderbuffer = nullptr;

    friend class CHyprOpenGLImpl;
};
