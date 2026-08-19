#pragma once
#include "PassElement.hpp"
#include "../Framebuffer.hpp"

namespace Render {
    class ITexture;
}

class CTextureMatteElement : public IPassElement {
  public:
    struct STextureMatteData {
        CBox                     box;
        SP<Render::ITexture>     tex;
        SP<Render::IFramebuffer> fb;
        bool                     disableTransformAndModify = false;
    };

    CTextureMatteElement(const STextureMatteData& data_);
    virtual ~CTextureMatteElement() = default;

    virtual bool        needsLiveBlur(const Render::CRenderingContext&);
    virtual bool        needsPrecomputeBlur(const Render::CRenderingContext&);

    virtual const char* passName() {
        return "CTextureMatteElement";
    }

    virtual ePassElementType type() {
        return EK_TEXTURE_MATTE;
    };

    STextureMatteData m_data;
};
