#pragma once

#include "../ElementRenderer.hpp"

namespace Render::GL {
    class CGLElementRenderer : public Render::IElementRenderer {
      public:
        CGLElementRenderer()  = default;
        ~CGLElementRenderer() = default;

      private:
        void draw(CRenderingContext& context, WP<CBorderPassElement> element, const Hyprutils::Math::CRegion& damage) override;
        void draw(CRenderingContext& context, WP<CClearPassElement> element, const CRegion& damage) override;
        void draw(CRenderingContext& context, WP<CFramebufferElement> element, const CRegion& damage) override;
        void draw(CRenderingContext& context, WP<CPreBlurElement> element, const CRegion& damage) override;
        void draw(CRenderingContext& context, WP<CRectPassElement> element, const CRegion& damage) override;
        void draw(CRenderingContext& context, WP<CShadowPassElement> element, const CRegion& damage) override;
        void draw(CRenderingContext& context, WP<CInnerGlowPassElement> element, const CRegion& damage) override;
        void draw(CRenderingContext& context, WP<CTexPassElement> element, const CRegion& damage) override;
        void draw(CRenderingContext& context, WP<CTextureMatteElement> element, const CRegion& damage) override;
    };
}
