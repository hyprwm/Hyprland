#pragma once
#include "PassElement.hpp"
#include "../Framebuffer.hpp"

class CTexture;

class CTextureMatteElement : public IPassElement {
  public:
    struct STextureMatteData {
        CBox             box;
        SP<CTexture>     tex;
        SP<CFramebuffer> fb;
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
    STextureMatteData data;
};