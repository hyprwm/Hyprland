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
#include "./pass/InnerGlowPassElement.hpp"
#include "./pass/TransformedWindowPassElement.hpp"
#include <hyprutils/math/Region.hpp>

namespace Render {
    class CRenderingContext;

    class IElementRenderer {
      public:
        IElementRenderer()          = default;
        virtual ~IElementRenderer() = default;

        void drawElement(CRenderingContext& context, WP<IPassElement> element, const CRegion& damage);

      protected:
        virtual void draw(CRenderingContext& context, WP<CBorderPassElement> element, const CRegion& damage)    = 0;
        virtual void draw(CRenderingContext& context, WP<CClearPassElement> element, const CRegion& damage)     = 0;
        virtual void draw(CRenderingContext& context, WP<CFramebufferElement> element, const CRegion& damage)   = 0;
        virtual void draw(CRenderingContext& context, WP<CPreBlurElement> element, const CRegion& damage)       = 0;
        virtual void draw(CRenderingContext& context, WP<CRectPassElement> element, const CRegion& damage)      = 0;
        virtual void draw(CRenderingContext& context, WP<CShadowPassElement> element, const CRegion& damage)    = 0;
        virtual void draw(CRenderingContext& context, WP<CInnerGlowPassElement> element, const CRegion& damage) = 0;
        virtual void draw(CRenderingContext& context, WP<CTexPassElement> element, const CRegion& damage)       = 0;
        virtual void draw(CRenderingContext& context, WP<CTextureMatteElement> element, const CRegion& damage)  = 0;

      private:
        void calculateUVForSurface(CRenderingContext& context, PHLWINDOW, SP<CWLSurfaceResource>, PHLMONITOR pMonitor, bool main = false, const Vector2D& projSize = {},
                                   const Vector2D& projSizeUnscaled = {}, bool fixMisalignedFSV1 = false);

        void drawRect(CRenderingContext& context, WP<CRectPassElement> element, const CRegion& damage);
        void drawHints(CRenderingContext& context, WP<CRendererHintsPassElement> element, const CRegion& damage);
        void drawPreBlur(CRenderingContext& context, WP<CPreBlurElement> element, const CRegion& damage);
        void drawClear(CRenderingContext& context, WP<CClearPassElement> element, const CRegion& damage);
        void drawSurface(CRenderingContext& context, WP<CSurfacePassElement> element, const CRegion& damage);
        void preDrawSurface(CRenderingContext& context, WP<CSurfacePassElement> element, const CRegion& damage);
        void drawTex(CRenderingContext& context, WP<CTexPassElement> element, const CRegion& damage);
        void drawTexMatte(CRenderingContext& context, WP<CTextureMatteElement> element, const CRegion& damage);
        void drawTransformedWindow(CRenderingContext& context, WP<CTransformedWindowPassElement> element, const CRegion& damage);
        void drawCustom(CRenderingContext& context, WP<IPassElement> element, const CRegion& damage);
    };
}
