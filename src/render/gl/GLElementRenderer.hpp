#pragma once

#include "../ElementRenderer.hpp"

namespace Render::GL {
    class CGLElementRenderer : public Render::IElementRenderer {
      public:
        CGLElementRenderer()  = default;
        ~CGLElementRenderer() = default;

      private:
        void draw(CRenderContext& ctx, WP<CBorderPassElement> element, const Hyprutils::Math::CRegion& damage) override;
        void draw(CRenderContext& ctx, WP<CClearPassElement> element, const CRegion& damage) override;
        void draw(CRenderContext& ctx, WP<CFramebufferElement> element, const CRegion& damage) override;
        void draw(CRenderContext& ctx, WP<CPreBlurElement> element, const CRegion& damage) override;
        void draw(CRenderContext& ctx, WP<CRectPassElement> element, const CRegion& damage) override;
        void draw(CRenderContext& ctx, WP<CShadowPassElement> element, const CRegion& damage) override;
        void draw(CRenderContext& ctx, WP<CInnerGlowPassElement> element, const CRegion& damage) override;
        void draw(CRenderContext& ctx, WP<CTexPassElement> element, const CRegion& damage) override;
        void draw(CRenderContext& ctx, WP<CTextureMatteElement> element, const CRegion& damage) override;
    };
}
