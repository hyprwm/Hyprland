#pragma once

#include "../ElementRenderer.hpp"

namespace Render::GL {
    class CGLElementRenderer : public Render::IElementRenderer {
      public:
        CGLElementRenderer()  = default;
        ~CGLElementRenderer() = default;

      private:
        void draw(WP<CBorderPassElement> element, const Hyprutils::Math::CRegion& damage);
        void draw(WP<CClearPassElement> element, const CRegion& damage);
        void draw(WP<CFramebufferElement> element, const CRegion& damage);
        void draw(WP<CPreBlurElement> element, const CRegion& damage);
        void draw(WP<CRectPassElement> element, const CRegion& damage);
        void draw(WP<CShadowPassElement> element, const CRegion& damage);
        void draw(WP<CTexPassElement> element, const CRegion& damage);
        void draw(WP<CTextureMatteElement> element, const CRegion& damage);
    };
}
