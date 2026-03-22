#pragma once

#include "../ElementRenderer.hpp"
#include "render/VKRenderer.hpp"

namespace Render::VK {
    class CVKElementRenderer : public Render::IElementRenderer {
      public:
        CVKElementRenderer(WP<CHyprVKRenderer> renderer);
        ~CVKElementRenderer() = default;

      private:
        void                draw(WP<CBorderPassElement> element, const Hyprutils::Math::CRegion& damage);
        void                draw(WP<CClearPassElement> element, const CRegion& damage);
        void                draw(WP<CFramebufferElement> element, const CRegion& damage);
        void                draw(WP<CPreBlurElement> element, const CRegion& damage);
        void                draw(WP<CRectPassElement> element, const CRegion& damage);
        void                draw(WP<CShadowPassElement> element, const CRegion& damage);
        void                draw(WP<CTexPassElement> element, const CRegion& damage);
        void                draw(WP<CTextureMatteElement> element, const CRegion& damage);

        WP<CHyprVKRenderer> m_renderer;
    };
}
