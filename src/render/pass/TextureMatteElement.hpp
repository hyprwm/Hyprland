#pragma once
#include "PassElement.hpp"

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

    virtual void draw(const CRegion& damage);
    virtual bool needsLiveBlur();
    virtual bool needsPrecomputeBlur();

  private:
    STextureMatteData data;
};