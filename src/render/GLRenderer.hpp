#pragma once

#include "Renderer.hpp"
#include "render/ElementRenderer.hpp"

namespace Render::GL {
    class IGLBlurProvider;

    class CHyprGLRenderer : public Render::IHyprRenderer {
      public:
        CHyprGLRenderer();
        ~CHyprGLRenderer();

        eType                   type() override;
        void                    endRender(CRenderingContext&, const std::function<void()>& renderingDoneCallback = {}) override;
        UP<ISyncFDManager>      createSyncFDManager() override;
        SP<ITexture>            createStencilTexture(const int width, const int height) override;
        SP<ITexture>            createTexture(bool opaque = false) override;
        SP<ITexture>            createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false) override;
        SP<ITexture>            createTexture(const Aquamarine::SDMABUFAttrs&, bool opaque = false) override;
        SP<ITexture>            createTexture(const int width, const int height, unsigned char* const data) override;
        SP<ITexture>            createTexture(cairo_surface_t* cairo) override;
        SP<ITexture>            createTexture(std::span<const float> lut3D, size_t N) override;
        bool                    explicitSyncSupported() override;
        bool                    fp16Supported() override;
        std::vector<SDRMFormat> getDRMFormats() override;
        std::vector<uint64_t>   getDRMFormatModifiers(DRMFormat format) override;
        SP<IFramebuffer>        createFB(const std::string& name = "") override;
        void                    disableScissor(CRenderingContext&) override;
        void                    blend(bool enabled) override;
        void                 drawShadow(CRenderingContext&, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) override;
        void                 drawShadow(CRenderingContext&, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                        const Config::CGradientValueData& grad2, float lerp, float a) override;

        void                 drawGlow(CRenderingContext&, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) override;
        void                 drawGlow(CRenderingContext&, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                      const Config::CGradientValueData& grad2, float lerp, float a) override;
        SP<IFramebuffer>     blurFramebuffer(CRenderingContext&, SP<IFramebuffer> source, float strength, const CRegion& originalDamage,
                                             const Render::SBlurContext& blurContext = {}) override;
        void                 refreshBlurProvider() override;
        void                 expandBlurDamage(CRegion& damage, float multiplier = 1.F) const override;
        bool                 blurProviderIsAnimated(const CRenderingContext&) const override;
        bool                 blurProviderRequiresLiveBlur() const override;
        void                 setViewport(int x, int y, int width, int height) override;
        bool                 reloadShaders(const std::string& path = "") override;

        void                 unsetEGL();
        WP<IElementRenderer> elementRenderer() override;

      private:
        void                 preRender(PHLMONITOR pMonitor);
        void                 renderOffToMain(CRenderingContext&, SP<IFramebuffer> off) override;
        SP<IRenderbuffer>    getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;
        bool                 beginRenderInternal(CRenderingContext&, CRegion& damage, bool simple = false) override;
        bool                 beginFullFakeRenderInternal(CRenderingContext&, CRegion& damage, SP<IFramebuffer> fb, bool simple = false) override;
        void                 initRender() override;
        bool                 initRenderBuffer(CRenderingContext&, SP<Aquamarine::IBuffer> buffer, uint32_t fmt) override;

        SP<ITexture>         getBlurTexture(const CRenderingContext&, PHLMONITORREF pMonitor) override;

        UP<IElementRenderer> m_elementRenderer;
        UP<IGLBlurProvider>  m_blur;
        CHyprSignalListener  m_preRenderListener;

        friend class CHyprOpenGLImpl;
    };
}
