#pragma once

#include "../../defines.hpp"
#include <vector>

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

namespace Render {
    class CRenderingContext;
}

class IPassElement {
  public:
    virtual ~IPassElement() = default;

    virtual std::vector<UP<IPassElement>> draw(Render::CRenderingContext& context);
    //
    virtual bool                needsLiveBlur(const Render::CRenderingContext&)       = 0;
    virtual bool                needsPrecomputeBlur(const Render::CRenderingContext&) = 0;
    virtual const char*         passName()                                            = 0;
    virtual ePassElementType    type()                                                = 0;
    virtual void                discard(Render::CRenderingContext& context);
    virtual bool                undiscardable();
    virtual std::optional<CBox> boundingBox(const Render::CRenderingContext& context); // in monitor-local logical coordinates
    virtual CRegion             opaqueRegion(const Render::CRenderingContext& context);
    virtual bool                disableSimplification();

    // cached results, computed once per frame in CRenderPass::render()
    bool needsLiveBlurCached       = false;
    bool needsPrecomputeBlurCached = false;
};
