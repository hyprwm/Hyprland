#pragma once

#include "../../defines.hpp"
#include <vector>

namespace Render {
    class CRenderContext;
}

enum ePassElementType : uint8_t {
    EK_UNKNOWN = 0,
    EK_BORDER,
    EK_CLEAR,
    EK_FRAMEBUFFER,
    EK_PRE_BLUR,
    EK_RECT,
    EK_HINTS,
    EK_SHADOW,
    EK_SURFACE,
    EK_TEXTURE,
    EK_TEXTURE_MATTE,
    EK_INNER_GLOW,
    EK_TRANSFORMED_WINDOW,
    EK_CUSTOM,
    EK_BACKDROP_SCOPE,
};

class IPassElement {
  public:
    virtual ~IPassElement() = default;

    virtual std::vector<UP<IPassElement>> draw(Render::CRenderContext& ctx);
    //
    virtual bool                needsLiveBlur(Render::CRenderContext& ctx)       = 0;
    virtual bool                needsPrecomputeBlur(Render::CRenderContext& ctx) = 0;
    virtual const char*         passName()                                       = 0;
    virtual ePassElementType    type()                                           = 0;
    virtual void                discard(Render::CRenderContext& ctx);
    virtual bool                undiscardable(Render::CRenderContext& ctx);
    virtual std::optional<CBox> boundingBox(Render::CRenderContext& ctx);  // in monitor-local logical coordinates
    virtual CRegion             opaqueRegion(Render::CRenderContext& ctx); // in monitor-local logical coordinates
    virtual bool                disableSimplification(Render::CRenderContext& ctx);
    virtual bool                requiresFullDamage(Render::CRenderContext& ctx);

    // cached results, computed once per frame in CRenderPass::render()
    bool needsLiveBlurCached       = false;
    bool needsPrecomputeBlurCached = false;
};
