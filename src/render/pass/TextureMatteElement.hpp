#pragma once
#include "PassElement.hpp"
#include "../Framebuffer.hpp"

class ITexture;

class CTextureMatteElement : public IPassElement {
  public:
    struct STextureMatteData {
        CBox             box;
        SP<ITexture>     tex;
        SP<IFramebuffer> fb;
        bool             disableTransformAndModify = false;
    };

    CTextureMatteElement(const STextureMatteData& data_);
    virtual ~CTextureMatteElement() = default;

    virtual void        draw(const CRegion& damage);
    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CTextureMatteElement";
    }

  private:
    STextureMatteData m_data;
};