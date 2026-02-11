#pragma once
#include "PassElement.hpp"
#include "../Framebuffer.hpp"

class ITexture;

class CTextureMatteElement : public IPassElement {
  public:
    struct STextureMatteData {
        CBox             box;
        SP<ITexture>     tex;
        SP<CFramebuffer> fb;
        bool             disableTransformAndModify = false;
    };

    CTextureMatteElement(const STextureMatteData& data_);
    virtual ~CTextureMatteElement() = default;

    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CTextureMatteElement";
    }

    virtual ePassElementKind kind() {
        return EK_TEXTURE_MATTE;
    };

    STextureMatteData m_data;
};