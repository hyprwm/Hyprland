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
    class IElementRenderer {
      public:
        IElementRenderer()          = default;
        virtual ~IElementRenderer() = default;

        void drawElement(CRenderContext& ctx, WP<IPassElement> element, const CRegion& damage);

      protected:
        virtual void draw(CRenderContext& ctx, WP<CBorderPassElement> element, const CRegion& damage)    = 0;
        virtual void draw(CRenderContext& ctx, WP<CClearPassElement> element, const CRegion& damage)     = 0;
        virtual void draw(CRenderContext& ctx, WP<CFramebufferElement> element, const CRegion& damage)   = 0;
        virtual void draw(CRenderContext& ctx, WP<CPreBlurElement> element, const CRegion& damage)       = 0;
        virtual void draw(CRenderContext& ctx, WP<CRectPassElement> element, const CRegion& damage)      = 0;
        virtual void draw(CRenderContext& ctx, WP<CShadowPassElement> element, const CRegion& damage)    = 0;
        virtual void draw(CRenderContext& ctx, WP<CInnerGlowPassElement> element, const CRegion& damage) = 0;
        virtual void draw(CRenderContext& ctx, WP<CTexPassElement> element, const CRegion& damage)       = 0;
        virtual void draw(CRenderContext& ctx, WP<CTextureMatteElement> element, const CRegion& damage)  = 0;

      private:
        void calculateUVForSurface(CRenderContext& ctx, PHLWINDOW, SP<CWLSurfaceResource>, PHLMONITOR pMonitor, bool main = false, const Vector2D& projSize = {},
                                   const Vector2D& projSizeUnscaled = {}, bool fixMisalignedFSV1 = false);

        void drawRect(CRenderContext& ctx, WP<CRectPassElement> element, const CRegion& damage);
        void drawHints(CRenderContext& ctx, WP<CRendererHintsPassElement> element, const CRegion& damage);
        void drawPreBlur(CRenderContext& ctx, WP<CPreBlurElement> element, const CRegion& damage);
        void drawClear(CRenderContext& ctx, WP<CClearPassElement> element, const CRegion& damage);
        void drawSurface(CRenderContext& ctx, WP<CSurfacePassElement> element, const CRegion& damage);
        void preDrawSurface(CRenderContext& ctx, WP<CSurfacePassElement> element, const CRegion& damage);
        void drawTex(CRenderContext& ctx, WP<CTexPassElement> element, const CRegion& damage);
        void drawTexMatte(CRenderContext& ctx, WP<CTextureMatteElement> element, const CRegion& damage);
        void drawTransformedWindow(CRenderContext& ctx, WP<CTransformedWindowPassElement> element, const CRegion& damage);
        void drawCustom(CRenderContext& ctx, WP<IPassElement> element, const CRegion& damage);
    };
}
