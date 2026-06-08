#pragma once

#include "../ElementRenderer.hpp"
#include "render/VKRenderer.hpp"

namespace Render::VK {
    class CVKElementRenderer : public Render::IElementRenderer {
      public:
        CVKElementRenderer(WP<CHyprVKRenderer> renderer);
        ~CVKElementRenderer() = default;

      private:
        void                draw(WP<CBorderPassElement> element, const Hyprutils::Math::CRegion& damage) override;
        void                draw(WP<CClearPassElement> element, const CRegion& damage) override;
        void                draw(WP<CFramebufferElement> element, const CRegion& damage) override;
        void                draw(WP<CPreBlurElement> element, const CRegion& damage) override;
        void                draw(WP<CRectPassElement> element, const CRegion& damage) override;
        void                draw(WP<CShadowPassElement> element, const CRegion& damage) override;
        void                draw(WP<CInnerGlowPassElement> element, const CRegion& damage) override;
        void                draw(WP<CTexPassElement> element, const CRegion& damage) override;
        void                draw(WP<CTextureMatteElement> element, const CRegion& damage) override;

        WP<CHyprVKRenderer> m_renderer;
    };
}
