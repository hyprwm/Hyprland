#pragma once

#include "./pass/BorderPassElement.hpp"
#include "./pass/ClearPassElement.hpp"
#include "./pass/FramebufferElement.hpp"
#include "./pass/PreBlurElement.hpp"
#include "./pass/RectPassElement.hpp"
#include "./pass/RendererHintsPassElement.hpp"
#include "./pass/ShadowPassElement.hpp"
#include "./pass/SurfacePassElement.hpp"
#include "./pass/TexPassElement.hpp"
#include "./pass/TextureMatteElement.hpp"
#include <hyprutils/math/Region.hpp>

namespace Render {
    class IElementRenderer {
      public:
        IElementRenderer()          = default;
        virtual ~IElementRenderer() = default;

        void drawElement(WP<IPassElement> element, const CRegion& damage);

      protected:
        virtual void draw(WP<CBorderPassElement> element, const CRegion& damage)   = 0;
        virtual void draw(WP<CClearPassElement> element, const CRegion& damage)    = 0;
        virtual void draw(WP<CFramebufferElement> element, const CRegion& damage)  = 0;
        virtual void draw(WP<CPreBlurElement> element, const CRegion& damage)      = 0;
        virtual void draw(WP<CRectPassElement> element, const CRegion& damage)     = 0;
        virtual void draw(WP<CShadowPassElement> element, const CRegion& damage)   = 0;
        virtual void draw(WP<CTexPassElement> element, const CRegion& damage)      = 0;
        virtual void draw(WP<CTextureMatteElement> element, const CRegion& damage) = 0;

      private:
        void calculateUVForSurface(PHLWINDOW, SP<CWLSurfaceResource>, PHLMONITOR pMonitor, bool main = false, const Vector2D& projSize = {}, const Vector2D& projSizeUnscaled = {},
                                   bool fixMisalignedFSV1 = false);

        void drawRect(WP<CRectPassElement> element, const CRegion& damage);
        void drawHints(WP<CRendererHintsPassElement> element, const CRegion& damage);
        void drawPreBlur(WP<CPreBlurElement> element, const CRegion& damage);
        void drawSurface(WP<CSurfacePassElement> element, const CRegion& damage);
        void preDrawSurface(WP<CSurfacePassElement> element, const CRegion& damage);
        void drawTex(WP<CTexPassElement> element, const CRegion& damage);
        void drawTexMatte(WP<CTextureMatteElement> element, const CRegion& damage);
        void drawCustom(WP<IPassElement> element, const CRegion& damage);
    };
}
